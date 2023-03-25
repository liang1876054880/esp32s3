
#ifndef __ARCH_NET_H__
#define __ARCH_NET_H__

#include "arch_chip.h"

#define IEEEtypes_SSID_SIZE             32
#define WLAN_PSK_MAX_LENGTH             64


typedef struct net_addr {
    uint32_t s_addr;
} net_addr_t;

#define NET_ADDR_INIT_VALUE             \
    {                                       \
        .s_addr = 0                         \
    }

typedef struct {
    net_addr_t ip;
    uint16_t port;
} net_ip_port_t;

#define NET_IP_PORT_INIT_VALUE          \
    {                                       \
        .ip = NET_ADDR_INIT_VALUE,          \
              .port = 0                           \
    }

#define MAKE_IPV4(a, b, c, d)           LONGVAL_LE(a, b, c, d)

typedef struct {
    net_addr_t ip;
    net_addr_t netmask;
    net_addr_t gw;
} ip_info_t;

#define IP_INFO_INIT_VALUE              \
    {                                       \
        .ip = NET_ADDR_INIT_VALUE,          \
              .netmask = NET_ADDR_INIT_VALUE,     \
                         .gw = NET_ADDR_INIT_VALUE           \
    }

typedef struct {
    uint8_t ssid[33];                   /**< SSID of target AP*/
    uint8_t password[65];               /**< password of target AP*/
    uint8_t bssid[6];                   /**< MAC address of target AP*/
    int8_t  primary;                    /**< channel of AP */
    int8_t  rssi;                       /**< signal strength of AP */
} net_wifi_ap_info_t;


uint64_t arch_net_htonll(uint64_t n);
uint64_t arch_net_ntohll(uint64_t n);

void arch_net_hton64_buf(uint64_t d64, uint8_t *p);
uint64_t arch_net_ntoh64_buf(const uint8_t *p);

char *arch_net_ntoa(net_addr_t *paddr);
char *arch_net_htoa(net_addr_t *paddr);
int arch_net_ntoa_buf(net_addr_t *paddr, char *buf, int buf_len);
int arch_net_htoa_buf(net_addr_t *paddr, char *buf, int buf_len);

int arch_net_name2ip(const char *name, net_addr_t *addr);
uint32_t arch_net_ip2n(const char *ip);

bool arch_net_is_offline(void);


int arch_net_get_ip_info(ip_info_t *ip_info);;
int arch_net_get_ap_info(net_wifi_ap_info_t *ap_info);
const char *arch_net_get_wifi_version(void);

#endif /* __ARCH_NET_H__ */

