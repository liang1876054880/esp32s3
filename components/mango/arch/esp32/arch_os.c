#include "arch_os.h"
#include "arch_dbg.h"

#undef ETSS_LOG_TAG
#define ETSS_LOG_TAG            "arch_os"

#define ARCH_OS_MS2TICK(ms)      ((ms)/portTICK_PERIOD_MS)
#define ARCH_OS_TICK2MS(tick)    ((tick)*portTICK_PERIOD_MS)

#define ARCH_OS_TIMER_OPT_BLOCK_MS_MAX      10*1000

#define portEND_SWITCHING_ISR( xSwitchRequired )    \
    do{ if( xSwitchRequired == pdTRUE)  portYIELD_FROM_ISR();}while(0)

#define portIsInIsr()            xPortInIsrContext()

static portMUX_TYPE port_mux = portMUX_INITIALIZER_UNLOCKED;

/*
   tick机制：
   通过tick_high 记录溢出
   */
static uint32_t tick_high = 0;          //tick高位 添加高位用于扩展计数范围
static bool tick_half_flag = false;     //tick值过半标记 超过0x7FFFFFFF 便设置为1

uint32_t arch_os_tick_now(void)
{
    uint32_t tick = xTaskGetTickCount();

    if (tick > 0x7FFFFFFF)
        tick_half_flag = true;  //过半标记

    else if (true == tick_half_flag) {  //若溢出 还没有标记
        tick_half_flag = false;
        tick_high++;
    }

    return tick;
}

void  arch_os_tick_sleep(uint32_t tick)
{
    vTaskDelay(tick);
}

uint32_t arch_os_tick_elapsed(uint32_t last_tick)
{
    uint32_t now = arch_os_tick_now();

    if (last_tick  < now)
        return now - last_tick;
    else//可能溢出
        return 0xFFFFFFFF - last_tick + now + 1;
}

uint32_t arch_os_ms_now(void)
{
    return (ARCH_OS_TICK2MS(arch_os_tick_now()));
}

void  arch_os_ms_sleep(uint32_t ms)
{
    vTaskDelay(ARCH_OS_WAIT_MS2TICK(ms));
}

uint32_t arch_os_ms_elapsed(uint32_t last_ms)
{
    return (ARCH_OS_TICK2MS(arch_os_tick_elapsed(ARCH_OS_MS2TICK(last_ms))));
}

/*
   time机制：
   以秒为单位，从开机为0计时.
   通过 time_tick_diff 记录实际一秒和系统tick值的差值；
   可以提供 校准之后的 秒时间
   可以设置 秒时间校准
*/

static int32_t time_tick_diff = 0;  //本地时间和实际时间的差值 正数表示表快 反之表慢

//读取校准过的 秒时间值 系统开启时间 参考为0
uint32_t arch_os_time_now(void)
{
    uint32_t t = 0;

    //若tick有溢出
    if (tick_high > 0) {
        t = 0xFFFFFFFF / (configTICK_RATE_HZ + time_tick_diff);
        t *= tick_high;
    }

    t += arch_os_tick_now() / (configTICK_RATE_HZ + time_tick_diff);
    return t;
}


//设置时间(用于调节系统时间)
void arch_os_time_tune(uint32_t std_time)
{
    uint32_t now = arch_os_time_now();
    int32_t diff = 0;

    if (std_time > now) {   //系统时间过慢
        uint32_t slow = std_time - now;     //计算 在now时间段之内 慢了多少秒
        diff = (slow * configTICK_RATE_HZ / now) * (-1);
        if (diff == 0)diff = -2;
    } else {    //系统时间过快
        uint32_t quick = now - std_time;
        diff = (quick * configTICK_RATE_HZ / now);
        if (diff == 0) diff = 2;
    }

    time_tick_diff += diff / 2;
}

/*
   utc机制:
   基于time机制 和 utc_offset 本地维持一个utc时钟
   设置utc 时间将用于校准time
*/
static uint32_t utc_offset = 0;

uint32_t arch_os_utc_now(void)
{
    return utc_offset + arch_os_time_now();
}


//通过utc标准时间 校准系统，
//返回系统时间偏移 (正数 表示系统时间快过UTC时间值)
int arch_os_utc_set(uint32_t utc)
{
    int ret = 0;
    uint32_t time_now = arch_os_time_now();

    if (utc_offset != 0) {      //非第一次读取标准时间

        ret = utc_offset + time_now - utc;      //本地时间 和 标准时间 差

        //暂时不调整斜率，保持简单策略
        //arch_os_time_tune(utc - utc_offset);  //用标准时间间隔 调整系统时间

        utc_offset = utc - time_now;        //再一次设置标准时间
    } else {
        utc_offset = utc - time_now;        //第一次设置标准时间
    }

    return ret;
}

int arch_os_utc_parse(uint32_t ts, uint16_t *year,
                      uint8_t *month, uint8_t *day,
                      uint8_t *hour, uint8_t *min, uint8_t *sec)
{
#define SECONDS_IN_365_DAY_YEAR  (31536000)
#define SECONDS_IN_A_DAY         (86400)
#define SECONDS_IN_A_HOUR        (3600)
#define SECONDS_IN_A_MINUTE      (60)
    uint32_t secondsPerMonth[12] = {
        31 * SECONDS_IN_A_DAY,
        28 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
        30 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
        30 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
        30 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
        30 * SECONDS_IN_A_DAY,
        31 * SECONDS_IN_A_DAY,
    };

    uint32_t time = ts;

    /* Calculate year */
    uint32_t tmp = 1970 + time / SECONDS_IN_365_DAY_YEAR;
    uint32_t number_of_leap_years = (tmp - 1968 - 1) / 4;
    time = time - ((tmp - 1970) * SECONDS_IN_365_DAY_YEAR);
    if (time > (number_of_leap_years * SECONDS_IN_A_DAY)) {
        time = time - (number_of_leap_years * SECONDS_IN_A_DAY);
    } else {
        time = time - (number_of_leap_years * SECONDS_IN_A_DAY) + SECONDS_IN_365_DAY_YEAR;
        --tmp;
    }

    if (year)
        *year = tmp;

    /* Remember if the current year is a leap year */
    uint8_t  is_a_leap_year = ((tmp - 1968) % 4 == 0);

    /* Calculate month */
    tmp = 1;
    int a;
    for (a = 0; a < 12; ++a) {

        uint32_t seconds_per_month = secondsPerMonth[a];
        /* Compensate for leap year */
        if ((a == 1) && is_a_leap_year)
            seconds_per_month += SECONDS_IN_A_DAY;

        if (time > seconds_per_month) {
            time -= seconds_per_month;
            tmp += 1;
        } else break;
    }

    if (month)*month = tmp;

    /* Calculate day */
    tmp = time / SECONDS_IN_A_DAY;
    time = time - (tmp * SECONDS_IN_A_DAY);
    ++tmp;
    if (day)*day = tmp;


    /* Calculate hour */
    tmp = time / SECONDS_IN_A_HOUR;
    time = time - (tmp * SECONDS_IN_A_HOUR);
    if (hour)*hour = tmp;

    /* Calculate minute */
    tmp = time / SECONDS_IN_A_MINUTE;
    time = time - (tmp * SECONDS_IN_A_MINUTE);
    if (min)*min = tmp;

    /* Calculate seconds */
    if (sec)*sec = time;

    return 0;
}

/*************************************************
 * remove mutex & semphre api
 **************************************************/

int arch_os_thread_create(arch_os_thread_handle_t *pthread, const char *name, arch_os_function_return_t (*function)(void *arg), uint32_t stack_size, void *arg, int priority)
{
    if (ARCH_OS_PRIORITY_DEFAULT == priority) {
        priority = ARCH_OS_NATIVE_PRIORITY_DEFAULT;
    }

    if (strstr(name, "etss_task")) {
        priority = configMAX_PRIORITIES;
    }
    if (pdPASS == xTaskCreate((pdTASK_CODE)function, (const char *const)name, (unsigned short)(stack_size / sizeof(portSTACK_TYPE)),
                              arg, priority, pthread)) {
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "create handle = %x, name = %s, prio = %d", (uint32_t)(*pthread), name, priority);
        return ETSS_OK;
    }

    LOG_ERROR_TAG(ETSS_LOG_TAG, "create failed, name = %s, prio = %d", name, priority);

    return ETSS_ERROR;
}


int arch_os_thread_delete(arch_os_thread_handle_t thread)
{
    if (NULL == thread)
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "delete handle = %x", (uint32_t)xTaskGetCurrentTaskHandle());
    else
        LOG_DEBUG_TAG(ETSS_LOG_TAG, "delete handle = %x", (uint32_t)thread);

    vTaskDelete(thread);
    return ETSS_OK;
}

void arch_os_thread_finish(void)
{
    vTaskDelete(NULL);
}

void arch_os_enter_critical(void)
{
    portENTER_CRITICAL(&port_mux);
}

void arch_os_exit_critical(void)
{
    portEXIT_CRITICAL(&port_mux);
}

void arch_os_schedule_stop(void)
{
    vTaskSuspendAll();
}

void arch_os_schedule_resume(void)
{
    xTaskResumeAll();
}

int arch_os_mbox_new(arch_os_mbox_handle_t *pmbox, int size)
{
    *pmbox = xQueueCreate(size, (uint32_t)sizeof(void *));
    if (NULL != *pmbox)
        return ETSS_OK;
    else
        return ETSS_ERROR;
}

int arch_os_mbox_free(arch_os_mbox_handle_t mbox)
{
    if (mbox) {
        vQueueDelete(mbox);
    }

    return ETSS_OK;
}

int arch_os_mbox_post(arch_os_mbox_handle_t mbox, void *msg, uint32_t wait_ms)
{
    if (NULL == mbox) {
        return ETSS_ERROR;
    }

    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xQueueSendFromISR(mbox, &msg, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xQueueSend(mbox, &msg, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

//wait = 0 会立即返回
int arch_os_mbox_fetch(arch_os_mbox_handle_t mbox, void **msg, uint32_t wait_ms)
{
    if (NULL == mbox) {
        return ETSS_ERROR;
    }

    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xQueueReceiveFromISR(mbox, msg, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xQueueReceive(mbox, msg, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    if (pdPASS == ret)
        return ETSS_OK;
    else if (wait_ms == 0)
        return ETSS_ERROR_EMPTY;
    else
        return ETSS_ERROR_TIMEOUT;
}

/****************OS-SEMAPHORE*********************/
int arch_os_semaphore_create(arch_os_semaphore_handle_t *pmhandle)
{
    *pmhandle = xSemaphoreCreateMutex();
    if (*pmhandle) {
        return ETSS_OK;
    } else {
        return ETSS_ERROR;
    }
}

int arch_os_semaphore_create_counting(arch_os_semaphore_handle_t *pmhandle, uint32_t maxcount, uint32_t initcount)
{
    *pmhandle = xSemaphoreCreateCounting(maxcount, initcount);
    if (*pmhandle) {
        return ETSS_OK;
    } else {
        return ETSS_ERROR;
    }
}

int arch_os_semaphore_get(arch_os_semaphore_handle_t mhandle, uint32_t wait_ms)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xSemaphoreTakeFromISR(mhandle, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xSemaphoreTake(mhandle, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_semaphore_put(arch_os_semaphore_handle_t mhandle)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xSemaphoreGiveFromISR(mhandle, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xSemaphoreGive(mhandle);
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_semaphore_getcount(arch_os_semaphore_handle_t mhandle)
{
    return uxQueueMessagesWaiting(mhandle);
}


int arch_os_semaphore_delete(arch_os_semaphore_handle_t mhandle)
{
    vSemaphoreDelete(mhandle);
    return ETSS_OK;
}

/****************OS-MUTEX*********************/

int arch_os_mutex_create(arch_os_mutex_handle_t *phandle)
{
    *phandle = xSemaphoreCreateMutex();
    if (*phandle) {
        return ETSS_OK;
    } else {
        return ETSS_ERROR;
    }
}

int arch_os_mutex_delete(arch_os_mutex_handle_t handle)
{
    vSemaphoreDelete(handle);
    return ETSS_OK;
}

int arch_os_mutex_put(arch_os_mutex_handle_t handle)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xSemaphoreGiveFromISR(handle, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xSemaphoreGive(handle);
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_mutex_get(arch_os_mutex_handle_t handle, uint32_t wait_ms)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xSemaphoreTakeFromISR(handle, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xSemaphoreTake(handle, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_queue_create(arch_os_queue_handle_t *pqhandle, uint32_t q_len, uint32_t item_size)
{
    *pqhandle = xQueueCreate(q_len, item_size);
    if (*pqhandle) {
        return ETSS_OK;
    } else {
        return ETSS_ERROR;
    }
}

int arch_os_queue_delete(arch_os_queue_handle_t qhandle)
{
    vQueueDelete(qhandle);
    return ETSS_OK;
}

int arch_os_queue_send(arch_os_queue_handle_t qhandle, const void *msg, uint32_t wait_ms)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xQueueSendToBackFromISR(qhandle, msg, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xQueueSendToBack(qhandle, msg, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_queue_recv(arch_os_queue_handle_t qhandle, void *msg, uint32_t wait_ms)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xQueueReceiveFromISR(qhandle, msg, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xQueueReceive(qhandle, msg, ARCH_OS_WAIT_MS2TICK(wait_ms));
    }

    return ret == pdTRUE ? ETSS_OK : ETSS_ERROR;
}

int arch_os_queue_get_msgs_waiting(arch_os_queue_handle_t qhandle)
{
    int nmsg = 0;
    nmsg = uxQueueMessagesWaiting(qhandle);
    return nmsg;
}

/********************OS-TIMER**********************/
int arch_os_timer_create(arch_os_timer_handle_t *ptimer, const char *name, uint32_t ms,
                         void (*call_back)(arch_os_timer_handle_t), void *context,
                         arch_os_timer_reload_t reload,
                         arch_os_timer_activate_t activate)
{
    int auto_reload = (reload == ARCH_OS_TIMER_ONE_SHOT) ? pdFALSE : pdTRUE;
    uint32_t ticks = ARCH_OS_WAIT_MS2TICK(ms);

    *ptimer = xTimerCreate((const char *const)name, ticks,
                           auto_reload, context, call_back);
    if (*ptimer == NULL)
        return ETSS_ERROR;

    if (activate == ARCH_OS_TIMER_AUTO_ACTIVATE) {
        if (pdTRUE != xTimerStart(*ptimer, ticks)) {
            xTimerDelete(*ptimer, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
            *ptimer = NULL;
            return ETSS_ERROR;
        }
    }

    return ETSS_OK;
}


int arch_os_timer_activate(arch_os_timer_handle_t timer)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xTimerStartFromISR(timer, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xTimerStart(timer, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
    }

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}

/** Change timer period
 *
 * This function changes the period of a timer that was previously created using
 * os_time_create(). This function changes the period of an active or dormant
 * state timer.
 *
 * @param[in] timer_t Pointer to a timer handle
 * @param[in] new_ms Time in ms after which the timer will expire
 *
 * @return ETSS_OK on success
 * @return ETSS_ERROR on failure
 */

int arch_os_timer_change(arch_os_timer_handle_t timer, uint32_t ms)
{
    int ret;
    uint32_t ticks = ARCH_OS_WAIT_MS2TICK(ms);
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xTimerChangePeriodFromISR(timer, ticks, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        /* Fixme: What should be value of xBlockTime? */
        ret = xTimerChangePeriod(timer, ticks, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
    }

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}

/** Check the timer active state
 *
 * This function checks if the timer is in the active or dormant state. A timer
 * is in the dormant state if (a) it has been created but not started, or (b) it
 * has expired and a one-shot timer.
 *
 * @param [in] timer_t Pointer to a timer handle
 *
 * @return ETSS_OK on success
 * @return ETSS_ERROR on failure
 */
int arch_os_timer_is_active(arch_os_timer_handle_t timer)
{
    int ret;

    ret = xTimerIsTimerActive(timer);

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}

void *arch_os_timer_get_context(arch_os_timer_handle_t timer)
{
    return (void *)pvTimerGetTimerID(timer);
}

/** Reset timer
 *
 * This function resets a timer that was previously created using using
 * os_timer_create(). If the timer had already been started and was already in
 * the active state, then this call will cause the timer to re-evaluate its
 * expiry time so that it is relative to when os_timer_reset() was called. If
 * the timer was in the dormant state then this call behaves in the same way as
 * os_timer_activate().
 *
 * @param[in] timer_t Pointer to a timer handle
 *
 * @return ETSS_OK on success
 * @return ETSS_ERROR on failure
 */
int arch_os_timer_reset(arch_os_timer_handle_t timer)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xTimerResetFromISR(timer, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xTimerReset(timer, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
    }

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}

/** Deactivate timer
 *
 * This function deactivates (or stops) a timer that was previously started.
 *
 * @param [in] timer_t handle populated by os_timer_create()
 *
 * @return ETSS_OK on success
 * @return ETSS_ERROR on failure
 */
int arch_os_timer_deactivate(arch_os_timer_handle_t timer)
{
    int ret;
    if (portIsInIsr()) {
        portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
        ret = xTimerStopFromISR(timer, &xHigherPriorityTaskWoken);
        portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
    } else {
        ret = xTimerStop(timer, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
    }

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}


int arch_os_timer_delete(arch_os_timer_handle_t timer)
{
    int ret;
    ret = xTimerDelete(timer, ARCH_OS_MS2TICK(ARCH_OS_TIMER_OPT_BLOCK_MS_MAX));
    if (pdPASS != ret) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "arch_os_timer_delete failed");
    }

    return ret == pdPASS ? ETSS_OK : ETSS_ERROR;
}


static void aysnc_callback(arch_os_timer_handle_t timer)
{
    struct {
        void (*cb)(void *);
        void *cb_arg;
    }*async_ctx = arch_os_timer_get_context(timer);

    if (async_ctx) {
        async_ctx->cb(async_ctx->cb_arg);
        free(async_ctx);
    }

    arch_os_timer_delete(timer);
}

int arch_os_async_call(void (*cb)(void *cb_arg), void *cb_arg, int delay_ms)
{
    if (NULL == cb)
        return ETSS_ERROR_PARAM;

    struct {
        void (*cb)(void *cb_arg);
        void *cb_arg;
    }*async_ctx = malloc(2 * sizeof(void *));

    async_ctx->cb = cb;
    async_ctx->cb_arg = cb_arg;

    int ret;
    arch_os_timer_handle_t async_timer;
    ret = arch_os_timer_create(&async_timer, "asyncCallTimer", delay_ms, aysnc_callback, async_ctx, ARCH_OS_TIMER_ONE_SHOT, ARCH_OS_TIMER_AUTO_ACTIVATE);

    if (ETSS_OK != ret) {
        free(async_ctx);
    }

    return ret;
}


void arch_os_reboot(void)
{
    esp_restart();
}

static void reboot_async(arch_os_timer_handle_t timer)
{
    arch_os_timer_delete(timer);
    esp_restart();
}

void arch_os_reboot_async(int delay_ms)
{
    arch_os_timer_handle_t async_timer;

    if (ETSS_OK != arch_os_timer_create(&async_timer, "asyncRebootTimer", delay_ms, reboot_async, NULL, ARCH_OS_TIMER_ONE_SHOT, ARCH_OS_TIMER_AUTO_ACTIVATE)) {
        esp_restart();
    }
}


uint32_t arch_os_get_free_heap_size()
{
    return esp_get_free_heap_size();
}

uint8_t *arch_os_mem_strstr(uint8_t *src, uint32_t src_len, uint8_t *sub, uint32_t sub_len)
{
    uint8_t *bp = NULL;
    uint8_t *sp = NULL;
    uint32_t bpoff_set = src_len - sub_len;
    uint32_t spoff_set = sub_len;

    if (NULL == src  || src_len <= 0 || NULL == sub || sub_len <= 0 || src_len < sub_len) {
        LOG_ERROR_TAG(ETSS_LOG_TAG, "invalid parameters");
        goto _err_exit;
    }

    while (bpoff_set > 0) {
        bp = src;
        sp = sub;
        spoff_set = sub_len;
        while (*bp == *sp) {
            spoff_set --;

            if (spoff_set == 0) {
                return src;
            } else {
                ++ bp;
                ++ sp;
            }
        }

        src ++;
        bpoff_set --;
    }

_err_exit:
    return NULL;
}

