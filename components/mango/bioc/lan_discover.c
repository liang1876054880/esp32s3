#include "bioc.h"
#include "bioc_iomux.h"
#include "lwip/sockets.h"

#define CTRL_PORT        43210
#define LAN_SERVER_PORT  55443
#define PNP_PORT         1982
#define DISC_OS_NAME     "bioc"
#define DISC_OS_VERSION  "1"
#define VENDOR_DOMAIN    ""
#define MESSAGE_DELIM    "\r\n"

typedef u32_t in_addr_t;

typedef struct {
    char          uuid[32];
    char          support_methods[512];
    uint32_t      myaddr;
    unsigned      cache_ctrl;
    unsigned      adv_cnt;
    bioc_tmr_t    adv_tmr;
} lan_disc_ctx_t;

static lan_disc_ctx_t disc_ctx;

typedef struct {
    bioc_tmr_hdl_t     tmr_hdl;
    bioc_iomux_hdl_t   iomux_hdl;
    int                uc_sock;
    bioc_io_inst_t     mcast_io;
} lan_disc_cb_t;
int errno;

static lan_disc_cb_t lan_disc_cb;
static char *get_vendor_header();
static int mcast_sock_recv(bioc_io_inst_t *);
static int lan_disc_send(int sock, unsigned short port,
                         in_addr_t to_addr, char *data, unsigned size);
static void adv_start(int tid, void *cb);
static int mcast_sock_create(uint32_t mcast_addr, uint16_t port);

int lan_discover_init(char *uuid, bioc_tmr_hdl_t thdl, bioc_tmr_hdl_t imhdl)
{
    int sock, len;
    char **method;

    lan_disc_cb.tmr_hdl = thdl;
    lan_disc_cb.iomux_hdl = imhdl;

    strncpy(disc_ctx.uuid, uuid, sizeof(disc_ctx.uuid) - 1);
    LOG_INFO("disc_ctx.uuid=%s, uuid=%s\r\n", disc_ctx.uuid, uuid);
    disc_ctx.cache_ctrl = 3600;

#if 0
    for (method = &bioc_supports[0], len = 0; *method != NULL ; method++) {
        len += snprintf(disc_ctx.support_methods + len,
                        sizeof(disc_ctx.support_methods) - len,
                        "%s ",
                        *method);
    }

    if (len)
        disc_ctx.support_methods[len - 1] = '\0'; /* remove the last wsp */
#endif

    sock = mcast_sock_create(inet_addr("239.255.255.250"), PNP_PORT);
    if (sock < 0) {
        LOG_INFO("Failed to create mCAST socket\r\n");
        return -1;
    }

    IO_INST_INIT(&(lan_disc_cb.mcast_io),
                 sock,
                 IO_EVENT_IN,
                 1,
                 mcast_sock_recv,
                 NULL,
                 NULL,
                 &(lan_disc_cb.mcast_io)
                );
    bioc_iomux_add(lan_disc_cb.iomux_hdl, &(lan_disc_cb.mcast_io));

    /* create unicast socket for discover response */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        LOG_ERROR("Failed to unicast discover socket\r\n");
        goto err;
    }

    lan_disc_cb.uc_sock = sock;
    LOG_INFO("unicast socket %d created\r\n", sock);

    /* start advertisment after 2 seconds */
    LOG_INFO("schedule self adv \r\n");

    bioc_start_tmr(lan_disc_cb.tmr_hdl,
                   &(disc_ctx.adv_tmr),
                   adv_start,
                   200 / MS_PER_TICK);

    return 0;
err:
    bioc_iomux_del(&(lan_disc_cb.mcast_io));
    return -1;
}

void lan_discover_deinit()
{
    bioc_iomux_del(&(lan_disc_cb.mcast_io));
    close(lan_disc_cb.uc_sock);
    bioc_del_timer(&(disc_ctx.adv_tmr));
}


static void adv_start(int tid, void *cb)
{
    char buf[1024];
    int len, rc;

    LOG_INFO("...adv_start called \r\n");

    if (disc_ctx.adv_cnt > 2) {
        disc_ctx.adv_cnt = 0;
        bioc_start_tmr(lan_disc_cb.tmr_hdl, &(disc_ctx.adv_tmr), adv_start, (disc_ctx.cache_ctrl * 100) / MS_PER_TICK);
        return;
    }

    disc_ctx.adv_cnt++;
    bioc_start_tmr(lan_disc_cb.tmr_hdl, &(disc_ctx.adv_tmr), adv_start, 300 / MS_PER_TICK);

    /*printf self mac and IP*/
    tcpip_adapter_ip_info_t local_ip;
    esp_err_t res_ap_get = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &local_ip);

    if (res_ap_get == ESP_OK) {
        LOG_INFO("get is success\n");
        LOG_WARN("now_self_ip:"IPSTR"\n", IP2STR(&local_ip.ip));
        LOG_DEBUG("now_self_netmask:"IPSTR"\n", IP2STR(&local_ip.netmask));
        LOG_ERROR("now_self_gw:"IPSTR"\n", IP2STR(&local_ip.gw));
     }

    /* update local address */
    disc_ctx.myaddr = local_ip.ip.addr;

    /* SSDP_ALIVE */
    len = snprintf(buf, sizeof(buf),
                   "NOTIFY * HTTP/1.1\r\n"
                   "Host: 239.255.255.250:%d\r\n"
                   "Cache-Control: max-age=%d\r\n"
                   "Location: bioc://%d.%d.%d.%d:%d\r\n"
                   "NTS: ssdp:alive\r\n"
                   "Server: POSIX, UPnP/1.0 %s/%s\r\n"
                   "%s",
                   PNP_PORT,
                   disc_ctx.cache_ctrl,
                   (uint8_t)(disc_ctx.myaddr),
                   (uint8_t)(disc_ctx.myaddr>>8),
                   (uint8_t)(disc_ctx.myaddr>>16),
                   (uint8_t)(disc_ctx.myaddr>>24),
                   LAN_SERVER_PORT,
                   DISC_OS_NAME,
                   DISC_OS_VERSION,
                   get_vendor_header());


    LOG_INFO("send ssdp:alive to multicast address len %d\r\n", len);

    rc = lan_disc_send(lan_disc_cb.mcast_io.fd, htons(PNP_PORT), 0, buf, len);
    if (rc < 0) {
        LOG_ERROR("Failed to send ssdp:alive to group\r\n");
    }

    return;
}


static int lan_disc_send(int sock, unsigned short port,
in_addr_t to_addr, char *data, unsigned size)
{
    struct sockaddr_in to;
    unsigned len;

    to.sin_family = AF_INET;
    to.sin_port = port;

    LOG_INFO("send message through sock %d to port %d\r\n", sock, port);

    if (!to_addr)
        to.sin_addr.s_addr = inet_addr("239.255.255.250");
    else
        to.sin_addr.s_addr = to_addr;

    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
                   (char *)&(disc_ctx.myaddr), sizeof(disc_ctx.myaddr)) == -1) {
        return -1;
    }

    len = sendto(sock, data, size, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));

    return len;
}

static int mcast_sock_recv(bioc_io_inst_t* inst)
{
    int rc;
    uint16_t len;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char buf[1024] = {0};
        struct sockaddr_in addr;

        rc = recvfrom(inst->fd, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &addr_len);
        if (rc <= 0) {
            LOG_ERROR("failed to receive from mcast socket\r\n");
            return 0;
        }

        if (memcmp(buf, "M-SEARCH * HTTP/1.1\r\n", strlen("M-SEARCH * HTTP/1.1\r\n"))) {
            LOG_ERROR("Invalid upnp discover message, ignore\r\n");
            return 0;
        }

        if (strstr(buf, "\"ssdp:discover\"\r\n") == NULL) {
            LOG_ERROR("Invalid upnp discover message, ignore\r\n");
            return 0;
        }

        if (strstr(buf, "bio_dev") == NULL) {
            LOG_ERROR("It's not for us\r\n");
            return 0;
        }

        LOG_INFO("Someone is trying to find us\r\n");

        /* update local address */
        /* disc_ctx.myaddr = local_ip.ip.addr; */

        len = snprintf(buf, sizeof(buf),
                       "HTTP/1.1 200 OK\r\n"
                       "Cache-Control: max-age=%d\r\n"
                       "Date: \r\n"
                       "Ext: \r\n"
                       "Location: bioc://%s:%d\r\n"
                       "Server: POSIX UPnP/1.0 %s/%s\r\n"
                       "%s",
                       disc_ctx.cache_ctrl,
                       inet_ntoa(disc_ctx.myaddr),
                       LAN_SERVER_PORT,
                       DISC_OS_NAME,
                       DISC_OS_VERSION,
                       get_vendor_header());

        /* send the response twice */
        rc = lan_disc_send(lan_disc_cb.uc_sock, addr.sin_port, addr.sin_addr.s_addr, buf, len);
        rc = lan_disc_send(lan_disc_cb.uc_sock, addr.sin_port, addr.sin_addr.s_addr, buf, len);

        return 0;
    }

#define ADD_HEADER_INT(header, val) len += snprintf(buf+len, sizeof(buf)-len, "%s"VENDOR_DOMAIN": %d\r\n", (header), (val))
#define ADD_HEADER_STR(header, val) len += snprintf(buf+len, sizeof(buf)-len, "%s"VENDOR_DOMAIN": %s\r\n", (header), (val))

static char* get_vendor_header()
{
    static char buf[512];
    int len = 0;

    ADD_HEADER_STR("id", disc_ctx.uuid);

    LOG_INFO("total len for vendor headers %d, contents:%s\r\n", len, buf);

    return buf;
}

static int mcast_sock_create(uint32_t mcast_addr, uint16_t port)
{
    int sock;
    int yes = 1;
    unsigned char ttl = 255;
    //uint8_t mcast_mac[6];
    struct sockaddr_in in_addr;
    struct ip_mreq mc;
    memset(&in_addr, 0, sizeof(in_addr));
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(port);
    in_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR("error: could not open multicast socket, errno sock: %d \r\n", sock);
        return -1;
    }
    LOG_INFO("multicast socket %d created\r\n", sock);

#ifdef SO_REUSEPORT_notused
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
                   sizeof(yes)) < 0) {
        LOG_ERROR("error: failed to set SO_REUSEPORT option, errno=%d\n", errno);
        close(sock);
        return -1;
    }
#endif

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

    if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
        LOG_ERROR("error: failed to bind to multicast address, errno=%d\r\n", errno);
        close(sock);
        return -1;
    }
    /* join multicast group */
    mc.imr_multiaddr.s_addr = mcast_addr;
    mc.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mc, sizeof(mc)) < 0) {
        LOG_ERROR("error: failed to join multicast group, errno=%d\r\n", errno);
        close(sock);
        return -1;
    }

    /* set other IP-level options */
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl,
                   sizeof(ttl)) < 0) {
        LOG_ERROR("error: failed to set multicast TTL, errno=%d\r\n", errno);
        close(sock);
        return -1;
    }
    return sock;
}
