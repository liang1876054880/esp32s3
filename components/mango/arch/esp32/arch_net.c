
#include "arch_dbg.h"
#include "arch_net.h"
#include "arch_os.h"
#include "misc/util.h"
#include "netdb.h"

uint64_t arch_net_htonll(uint64_t n)
{
    uint32_t high = n >> 32;
    uint32_t low = n;

    return (((uint64_t)htonl(low)) << 32) | htonl(high);

}

uint64_t arch_net_ntohll(uint64_t n)
{
    return arch_net_htonll(n);
}

static char *net_addr_ntoa(uint32_t addr)
{
    static char str[64] = {0};

    snprintf_safe(str, sizeof(str), "%d.%d.%d.%d", (addr >> 0) & 0xff, (addr >> 8) & 0xff, (addr >> 16) & 0xff, (addr >> 24) & 0xff);

    return str;
}



char *arch_net_ntoa(net_addr_t *paddr)
{
    return net_addr_ntoa(paddr->s_addr);
}

char *arch_net_htoa(net_addr_t *paddr)
{
    return net_addr_ntoa(htonl(paddr->s_addr));
}


int arch_net_ntoa_buf(net_addr_t *paddr, char *buf, int buf_len)
{
    int len;
    uint32_t addr = paddr->s_addr;
    len = snprintf_safe(buf, buf_len, "%d.%d.%d.%d", (addr >> 0) & 0xff, (addr >> 8) & 0xff, (addr >> 16) & 0xff, (addr >> 24) & 0xff);

    return len;
}

int arch_net_htoa_buf(net_addr_t *paddr, char *buf, int buf_len)
{
    int len;
    uint32_t addr = paddr->s_addr;
    len = snprintf_safe(buf, buf_len, "%d.%d.%d.%d", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, (addr >> 0) & 0xff);

    return len;
}

void arch_net_hton64_buf(uint64_t d64, uint8_t *p)
{
    int i;
    uint8_t *from = (uint8_t *)&d64;
    if (12345 == htonl(12345)) {
        for (i = 0; i < sizeof(uint64_t); i++)
            p[i] = from[i];
    } else {
        for (i = 0; i < sizeof(uint64_t); i++)
            p[i] = from[sizeof(uint64_t) - 1 - i];
    }
}

uint64_t arch_net_ntoh64_buf(const uint8_t *p)
{
    int i;
    uint64_t d64;
    uint8_t *to = (uint8_t *)&d64;
    if (12345 == htonl(12345)) {
        for (i = 0; i < sizeof(uint64_t); i++)
            to[i] = p[i];
    } else {
        for (i = 0; i < sizeof(uint64_t); i++)
            to[i] = p[sizeof(uint64_t) - 1 - i];
    }
    return d64;
}

int arch_net_name2ip(const char *name, net_addr_t *addr)
{

    struct hostent hostbuf, *hp;
    size_t hstbuflen;
    char *tmphstbuf;
    int res;
    int herr;

    hstbuflen = 1024;

    /* Allocate buffer, remember to free it to avoid memory leakage.  */
    tmphstbuf = malloc(hstbuflen);

    if (NULL == tmphstbuf) {
        return ETSS_ERROR_NOMEM;
    }

    while ((res = gethostbyname_r(name, &hostbuf, tmphstbuf, hstbuflen, &hp, &herr)) == ERANGE) {

        /* Enlarge the buffer.  */
        hstbuflen *= 2;

        tmphstbuf = realloc(tmphstbuf, hstbuflen);

        if (NULL == tmphstbuf) {
            return ETSS_ERROR_NOMEM;
        }

    }

    /*  Check for errors.  */
    if (0 == res && hp) {

        addr->s_addr = ntohl(*((uint32_t *)(hp->h_addr_list[0])));

        free(tmphstbuf);

        return ETSS_OK;
    }


    free(tmphstbuf);

    return ETSS_ERROR;

}


uint32_t arch_net_ip2n(const char *ip)
{
    struct in_addr addr = {0};
    if (0 != inet_aton(ip, &addr)) {
        return addr.s_addr;
    }
    return 0;
}

int arch_net_get_ip_info(ip_info_t *ip_info)
{
    tcpip_adapter_if_t esp_if;
    tcpip_adapter_ip_info_t esp_ip_info;

    wifi_mode_t wifi_mode = WIFI_MODE_STA;

    esp_wifi_get_mode(&wifi_mode);

    if (WIFI_MODE_AP == wifi_mode) {
        esp_if = TCPIP_ADAPTER_IF_AP;
    } else {
        esp_if = TCPIP_ADAPTER_IF_STA;
    }

    if (ESP_OK != tcpip_adapter_get_ip_info(esp_if, &esp_ip_info))
        return ETSS_ERROR;

    ip_info->ip.s_addr = ntohl(esp_ip_info.ip.addr);
    ip_info->netmask.s_addr = ntohl(esp_ip_info.netmask.addr);
    ip_info->gw.s_addr = ntohl(esp_ip_info.gw.addr);

    return ETSS_OK;
}

bool arch_net_is_offline(void)
{
    tcpip_adapter_ip_info_t esp_ip_info;

    if (ESP_OK != tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &esp_ip_info))
        return true;

    if (esp_ip_info.ip.addr != 0) {
        return false;
    } else {
        return true;
    }
}

int arch_net_get_ap_info(net_wifi_ap_info_t *ap_info)
{
    wifi_ap_record_t ap_record;

    if (ESP_OK == esp_wifi_sta_get_ap_info(&ap_record)) {

        memcpy(ap_info->ssid, ap_record.ssid, sizeof(ap_info->ssid));
        ap_info->ssid[sizeof(ap_info->ssid) - 1] = '\0';
        memcpy(ap_info->bssid, ap_record.bssid, sizeof(ap_info->bssid));
        ap_info->primary = ap_record.primary;
        ap_info->rssi = ap_record.rssi;

        return ETSS_OK;
    }

    return ETSS_ERROR;

}

const char *arch_net_get_wifi_version(void)
{
    return esp_get_idf_version();
}


