#ifndef _LAN_CTRL_H_
#define _LAN_CTRL_H_

#include <lwip/sockets.h>
#include <lwip/arch.h>
#include <inttypes.h>
#include <bioc_timer.h>
#include <bioc_iomux.h>

#define CTRL_PORT        43210
#define LAN_SERVER_PORT  55443
#define PNP_PORT         1982
#define DISC_OS_NAME     "bioc"
#define DISC_OS_VERSION  "1"
#define VENDOR_DOMAIN    ""
#define MESSAGE_DELIM    "\r\n"

#define CTRL_CONTENT_LEN 512
#define MAX_ALLOW_CLNT         4
#define QUOTA_PER_PERIOD       60

#define QUOTA_REFILL_INTERVAL  (60*1000) /* 1 minute */
#define TOTAL_QUOTA_PER_PERIOD (((MAX_ALLOW_CLNT * QUOTA_PER_PERIOD) * 60) / 100)

#define ERR_OK             0
#define ERR_INVALID_CMD    1
#define ERR_NOMETHOD       2
#define ERR_INVALID_PARAM  3
#define ERR_NO_MEM         4
#define ERR_QUTOA_EXCEED   5
#define ERR_GENERAL        6

typedef unsigned short u16;


typedef struct {
    struct sockaddr_in addr;
    int                addr_len;
    int                sock;
} ctrl_sock_t;

typedef struct {
    unsigned char         init;
    unsigned char         started;
    xTaskHandle           task;
    bioc_tmr_hdl_t        tmr_hdl;
    bioc_iomux_hdl_t      iomux_hdl;
    unsigned char         nr_clnt;
    list_head_t           clnt_list;
    bioc_io_inst_t        ctrl_io;
    bioc_io_inst_t        server_io;
    ctrl_sock_t           ctrl_sock;
    void                 *ctrl_sock_sem;
    unsigned              total_quota_remain;
    bioc_tmr_t            quota_refill_tmr;
} lan_ctrl_cb_t;

typedef enum {
    LAN_CLNT_NORMAL = 0,
    LAN_CLNT_MUSIC,
    LAN_CLNT_UDP,
    LAN_CLNT_UDP_MUSIC
} lan_clnt_type_t;

typedef struct {
    struct sockaddr_in    addr;
    socklen_t             addr_len;
    list_head_t           self;
    unsigned              id;
    bioc_io_inst_t        io;
    bioc_tmr_t            actmr;         /* access control timer */
    unsigned              quota_remain;
    char                  msg_buf[1024];
    unsigned short        msg_offset;
    lan_clnt_type_t       type;
} lan_clnt_t;


typedef struct {
    unsigned              clnt_id;
    lan_clnt_type_t       clnt_type;
    unsigned long         cmd_id;
} lan_cmd_ctx_t;

typedef enum {
    CTRL_CMD_START,
    CTRL_CMD_STOP,
    CTRL_CMD_START_MUSIC,
    CTRL_CMD_STOP_MUSIC,
    CTRL_CMD_DATA,
    CTRL_CMD_DESTROY,
    CTRL_CMD_MAX,
} ctrl_cmd_type_t;

typedef struct {
    ctrl_cmd_type_t       cmd;
    union {
        struct {
            unsigned      clnt_id;
            char          contents[1024];
        } data;
        struct {
            char          ip[16];
            u16           port;
        } mctrl;
    } u;
} ctrl_cmd_t;

extern void lan_ctrl_init();
extern void lan_ctrl_disable();
#ifdef YEE_RELEASE_MORE_MEMORY
extern void lan_ctrl_disable_for_ota();
#endif//YEE_RELEASE_MORE_MEMORY
extern void lan_ctrl_enable();
extern int lan_ctrl_stop_music();
extern int lan_ctrl_start_music(const char *, u16);
extern int lan_discover_init(char *, bioc_tmr_hdl_t, bioc_tmr_hdl_t);
extern void lan_discover_deinit();
extern int ctrl_cmd_send(ctrl_cmd_t *);
extern void lan_report_push_str(char *, char *);
extern void lan_report_push_sint(char *, int);
extern void lan_report_flush();

extern int call_method(lan_clnt_t *clnt, unsigned id, char *method, char *params, int direct);
extern void bioc_try_report_temp_data(temp_data_t *temp);

static inline int net_get_sock_error(int sock)
{
    switch (errno) {
    case EWOULDBLOCK:
        return -EAGAIN;
    case EBADF:
        return -EBADF;
    case ENOBUFS:
        return -ENOMEM;
    default:
        return errno;
    }
}


#endif
