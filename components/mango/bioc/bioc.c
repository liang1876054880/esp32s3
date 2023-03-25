#include "bioc.h"
#include "lan_ctrl.h"
#include "arch_os.h"

#define BIOC_MAX_REQUSTS        10
#define BIOC_POOL_ITEMS         (BIOC_MAX_REQUSTS)
#define BIOC_QUEUE_WAITING_TIME (0)

typedef struct {
    void    *async_task;
    void    *req_queue;
    uint8_t req_chk_tbl[10][BIOC_REQ_TYPE_MAX];
} bioc_cb_t;

typedef struct {
    uint32_t occupied;
    uint8_t  buffer[sizeof(bioc_request_t)];
} bioc_buffer_t;

static void *bioc_req_handler(void *);
static bioc_errno_t bioc_proc_request(const bioc_request_t *, void *);
static int bioc_base_modules_init(void);

static bioc_cb_t bioc_cb;
static bioc_cb_t *bioc_cp = &bioc_cb; /* bioc control block */
bioc_tmr_hdl_t bioc_tmr_hdl = NULL;
arch_os_thread_handle_t bioc_thread_handle;

static bioc_buffer_t bioc_buffer_pool[BIOC_POOL_ITEMS];

void bioc_buffer_pool_init(void)
{
    memset(bioc_buffer_pool, 0, sizeof(bioc_buffer_pool));
}

bioc_buffer_t *bioc_buffer_pool_alloc(void)
{
    UBaseType_t uxSavedInterruptStatus;
    uint32_t index;

    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    for (index = 0; index < 5; index++) {
        if (!bioc_buffer_pool[index].occupied) {
            bioc_buffer_pool[index].occupied = true;
            portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);
            return &bioc_buffer_pool[index];
        }
    }

    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);

    return NULL;
}

void bioc_buffer_pool_release(bioc_buffer_t *bioc_buffer)
{
    UBaseType_t uxSavedInterruptStatus;

    uxSavedInterruptStatus = portSET_INTERRUPT_MASK_FROM_ISR();
    bioc_buffer->occupied = false;
    portCLEAR_INTERRUPT_MASK_FROM_ISR(uxSavedInterruptStatus);
}

/**
 * func: send data to queue
 * parm:
 * return: true or fail
 */
int bioc_queue_send(bioc_buffer_t **bioc_buffer)
{
    int ret = pdTRUE;

    ret = xQueueSend(bioc_cp->req_queue, bioc_buffer, BIOC_QUEUE_WAITING_TIME);
    if (ret != pdTRUE) {
        bioc_buffer_pool_release(*bioc_buffer);
    }

    return (ret == pdTRUE) ? 0 : -1;
}

static int bioc_prod_init()
{
    bioc_prod_init_func_t *fn;
    int rc;

    for (fn = &bioc_prod_init_tbl[0]; *fn != NULL; fn++) {
        rc = (*fn)();
        if (rc < 0) {
            LOG_ERROR("bioc product specific init failed.\r\n");
            return rc;
        }
    }

    return 0;
}

/**
 * bioc base modules init such as timer cron queue.
 *             must be called at early stage.
 *
 * @return  - <0 init fail
 *          - =0 init success
 */
static int bioc_base_modules_init(void)
{
    int rc = 0;

    bioc_tmr_hdl = bioc_timer_init();
    if (!bioc_tmr_hdl) {
        LOG_ERROR("Failed to init bioc timer control.\r\n");
        return -1;
    }

    bioc_buffer_pool_init();
    /* create freeRTOS Queue */
    bioc_cp->req_queue = xQueueCreate(BIOC_MAX_REQUSTS, sizeof(bioc_request_t *));
    LOG_INFO("init bioc_cp->req_queue: %p\r\n", bioc_cp->req_queue);

    if (bioc_cp->req_queue == NULL) {
        LOG_ERROR("[bioc]create queue failed!\r\n");
        return -1;
    }

    /* create bioc request handling task */
    arch_os_thread_create(&bioc_thread_handle,
                          "bioc_task",
                          bioc_req_handler,
                          1024 * 4,
                          NULL,
                          ARCH_OS_PRIORITY_DEFAULT);

    return rc;
}

int bioc_init_async(void)
{
    int rc = 0;

    //must be called at early stage.
    rc = bioc_base_modules_init();
    if (rc < 0) {
        return rc;
    }

    // product initialization after init_power_opt feature.
    rc = bioc_prod_init();
    if (rc < 0) {
        LOG_ERROR("Failed to init product specific control structure.\r\n");
        return rc;
    }

    return rc;
}

int bioc_post_request_async(bioc_req_type_t type, const void *req, unsigned int size, prot_rsp_cb_t routine, const void *priv)
{
    bioc_request_t *bioc_req = NULL;
    bioc_buffer_t  *bioc_buffer = bioc_buffer_pool_alloc();

    if (NULL == bioc_buffer) {
        /* LOG_ERROR("No bioc buffer, req:%d is dropped!\r\n", type); */
        return -1;
    }

    bioc_req = (bioc_request_t *)bioc_buffer->buffer;
    bioc_req->type = type;
    if (req) {
        memcpy(&(bioc_req->msg), req, size);
    }

    bioc_req->cb_func = routine;
    bioc_req->priv = priv;

    return bioc_queue_send(&bioc_buffer);
}

bioc_errno_t BIOC_DFLT_IMPL bioc_proc_temp_data(const temp_data_t *data)
{
    return BIOC_ERR_OK;
}

/*
 * PROC_PERIPHERAL default impl
 */
bioc_errno_t BIOC_DFLT_IMPL bioc_proc_peripheral(const bioc_peripheral_req_t *req)
{
    return BIOC_ERR_OK;
}

bioc_errno_t BIOC_DFLT_IMPL bioc_proc_uart_payload(const uart_payload_t *req)
{
    return BIOC_ERR_OK;
}

static bioc_errno_t bioc_proc_request(const bioc_request_t *req, void *rsp)
{
    bioc_errno_t err = BIOC_ERR_OK;

    /* LOG_INFO("Execute bioc request: %d\r\n", req->type); */

    switch (req->type) {
    case BIOC_ON_GOT_IP:
        LOG_INFO("enter lan-ctrl init\r\n");
        lan_ctrl_init();
        //lan_ctrl_start();
        break;
    case BIOC_REQ_DUMMY:
        err = BIOC_ERR_OK;
        break;
    case BIOC_REQ_UART_PAYLOAD:
        err = bioc_proc_uart_payload(&(req->msg.uart_payload));
        break;
    case BIOC_REQ_TEMP_CHECK:
        bioc_proc_temp_data((temp_data_t *)(&req->msg));
        break;

    case BIOC_PERIPHERAL_EVT:
        err = bioc_proc_peripheral(&(req->msg.peripheral_req));
        break;
    default:
        LOG_INFO("Unsupported request %d, drop it.\r\n", req->type);
        err = BIOC_ERR_GENERAL;
        break;
    }

    return err;
}

/**
 * func: receive data from queue
 * parm:
 * return: true or fail
 */
int bioc_queue_receive(bioc_buffer_t **bioc_buffer, unsigned long long wait)
{
    BaseType_t ret_val = xQueueReceive(bioc_cp->req_queue, bioc_buffer, wait);
    return (pdPASS == ret_val) ? 0 : -1;
}

static void *bioc_req_handler(void *args)
{
    int rc;
    bioc_errno_t err;
    bioc_request_t *req;
    bioc_ms_t val;
    unsigned long long wait;

    bioc_ms_t now_ms;
    bioc_ms_t next_status_sync_ms = arch_os_ms_now() + 1000;
    bioc_buffer_t *bioc_buffer = NULL;

    union {
        int setting_rsp;
    } rsp;

    //UNUSED_PARAM(args);
    LOG_INFO("bioc task started, wait msg ...\r\n");
    for (;;) {
        rc = bioc_get_next_timeout(bioc_tmr_hdl, &val);

        if (rc < 0) {
            wait = ARCH_OS_WAIT_FOREVER;
        } else if (val == 0) {
            wait = ARCH_OS_NO_WAIT;
        } else {
            wait = ARCH_OS_WAIT_MS2TICK(val);
        }

        rc = bioc_queue_receive(&bioc_buffer, wait);

        /* update current time after wakeup */
        bioc_time_update(bioc_tmr_hdl);
        bioc_proc_timer(bioc_tmr_hdl);

        if (rc == 0) {
            req = (bioc_request_t *)(bioc_buffer->buffer);
            memset(&rsp, 0, sizeof(rsp));
            err = bioc_proc_request(req, (void *)&rsp);
            if (req->cb_func) {
                req->cb_func(err, &rsp, (void *)req->priv);
            }

            bioc_buffer_pool_release(bioc_buffer);
        }

        now_ms = arch_os_ms_now();
        // LOG_INFO("%lld, %lld\r\n", now_ms, next_status_sync_ms);

        if ((now_ms >= next_status_sync_ms) || (rc == 0)) {
            next_status_sync_ms = now_ms + 200;
        }
    }

    return NULL;
}

void bioc_task_timer_update(void)
{
    BIOC_POST_REQUEST(BIOC_REQ_DUMMY, NULL, 0, NULL, NULL);
}
