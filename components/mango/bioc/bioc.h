#ifndef _BIOC_H_
#define _BIOC_H_

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "bioc_timer.h"
#include "arch_dbg.h"
/*
 * bioc Control error
 */
typedef enum {
    BIOC_ERR_OK,                /* no err */
    BIOC_ERR_INVALID,           /* invalid parameter */
    BIOC_ERR_HARDWARE,          /* hardware error */
    BIOC_ERR_TIMEOUT,           /* operation timeout */
    BIOC_ERR_GENERAL,           /* general error */
    BIOC_ERR_NOMEM,             /* no memory */
    BIOC_ERR_REJ,               /* invalid state */
    BIOC_ERR_INTERNAL,          /* intern error */
    BIOC_ERR_MAX,
} bioc_errno_t;

#define STATE_OK            0                   /* There is no error                        */
#define STATE_ERROR         (-1)                /* A generic error happens                  */
#define STATE_TIMEOUT       (-2)                /* Timed out                                */
#define STATE_FULL          (-3)                /* The resource is full                     */
#define STATE_EMPTY         (-4)                /* The resource is empty                    */
#define STATE_NOMEM         (-5)                /* No memory                                */
#define STATE_NOSYS         (-6)                /* No system                                */
#define STATE_BUSY          (-7)                /* Busy                                     */
#define STATE_TRYOUT        (-8)                /* try enough times                     */
#define STATE_NOTFOUND      (-9)
#define STATE_PARAM         (-10)
#define STATE_ERR_SIZE      (-11)

#define UNUSED_PARAM        (void *)

#define portIsInIsr()       xPortInIsrContext()

#define BIOC_DFLT_IMPL __attribute__((weak))

/*
 *  bioc   Control type enum
 */
typedef enum {
    BIOC_LAN_CTRL_EVT,
    BIOC_LAN_NOTIFY,
    BIOC_LAN_READABLE,
    BIOC_ON_GOT_IP,
    BIOC_PERIPHERAL_EVT,
    BIOC_REQ_DUMMY,
    BIOC_REQ_TEMP_CHECK,
    BIOC_REQ_UART_PAYLOAD,
    BIOC_REQ_TYPE_MAX,
} bioc_req_type_t;

typedef struct {
    double ch0_temp;
    double ch1_temp;
    double ch2_temp;
    double ch3_temp;
    double ch4_temp;
    double ch5_temp;
} temp_data_t;

typedef struct {
    int run;
    int clamp;
    int cycle_cnt;
} uart_payload_t;

typedef enum {
    PERIPHERAL_EVT_KEY,
    PERIPHERAL_EVT_KNOB,
    PERIPHERAL_EVT_LCD,
    PERIPHERAL_EVT_MAX,
} evt_peripheral_t;

typedef enum {
    PERIPHERAL_TYPE_TIME,
    PERIPHERAL_TYPE_NET,
    PERIPHERAL_TYPE_TEMP,
    PERIPHERAL_TYPE_BOOTING,
    PERIPHERAL_TYPE_KNOB,
    PERIPHERAL_TYPE_MAX,
} type_peripheral_t;

typedef struct {
    evt_peripheral_t  event;
    type_peripheral_t type;
    int      data[128];
} bioc_peripheral_req_t;

typedef void (*prot_rsp_cb_t)(bioc_errno_t, const void *, void *);
/*
 * bioc   Control Msg
 */
typedef struct {
    bioc_req_type_t   type;
    prot_rsp_cb_t     cb_func;
    const void       *priv;             /* priv will be passed back in cb_func */

    union {
        int power_on_req;
        temp_data_t temp;
        bioc_peripheral_req_t peripheral_req;
        uart_payload_t uart_payload;
    } msg;
} bioc_request_t;

typedef int (*bioc_prod_init_func_t)(void);
extern bioc_prod_init_func_t bioc_prod_init_tbl[];

/******************************************************************/
/**** bioc   Control Programming Interface *********/
/******************************************************************/
/*
 * Init bioc   Control service
 */
#define BIOC_INIT()    bioc_init_async()

/*
 * Request bioc   Control service
 */
#define BIOC_POST_REQUEST(type, req, size, routine, priv) \
    bioc_post_request_async(type, req, size, routine, priv)

int bioc_init_async(void);

int bioc_post_request_async(bioc_req_type_t type,
                            const void *req,
                            unsigned int size,
                            prot_rsp_cb_t routine,
                            const void *priv);

void bioc_task_timer_update(void);

bioc_errno_t bioc_proc_peripheral(const bioc_peripheral_req_t *req);
bioc_errno_t bioc_proc_temp_data(const temp_data_t *data);

extern bioc_tmr_hdl_t bioc_tmr_hdl;

#endif
