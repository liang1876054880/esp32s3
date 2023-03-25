#ifndef __ARCH_OS_H__
#define __ARCH_OS_H__

#include "arch_chip.h"

#define ARCH_OS_PRIORITY_DEFAULT            (-1)
#define ARCH_OS_NATIVE_PRIORITY_DEFAULT     (10)

#define ARCH_OS_WAIT_FOREVER                (0xFFFFFFFF)
#define ARCH_OS_NO_WAIT                     (0)
#define ARCH_OS_WAIT_MS2TICK(ms)            \
    ( ((ms) == ARCH_OS_WAIT_FOREVER) ? ARCH_OS_WAIT_FOREVER : (((ms)/portTICK_PERIOD_MS)+((((ms)%portTICK_PERIOD_MS)+portTICK_PERIOD_MS-1)/portTICK_PERIOD_MS)) )

typedef void                               *arch_os_semaphore_handle_t;
typedef void                              *arch_os_mutex_handle_t;
typedef void                               *arch_os_mbox_handle_t;
typedef void                              *arch_os_thread_handle_t;
typedef void                               *arch_os_queue_handle_t;
typedef void                               *arch_os_timer_handle_t;
typedef void                               *arch_os_function_return_t;
#define ARCH_OS_FUNCTION_RETURN(v)          ((arch_os_function_return_t)(v))

uint32_t arch_os_time_now(void);
void arch_os_time_tune(uint32_t std_time);

uint32_t arch_os_utc_now(void);
int arch_os_utc_set(uint32_t utc);
int arch_os_utc_parse(uint32_t ts, uint16_t *year, uint8_t *month, uint8_t *day, uint8_t *hour, uint8_t *min, uint8_t *sec);

uint32_t arch_os_tick_now(void);
void  arch_os_tick_sleep(uint32_t tick);
uint32_t arch_os_tick_elapsed(uint32_t last_tick);

uint32_t arch_os_ms_now(void);
void  arch_os_ms_sleep(uint32_t ms);
uint32_t arch_os_ms_elapsed(uint32_t last_ms);

arch_os_thread_handle_t arch_os_thread_current(void);
int arch_os_thread_create(arch_os_thread_handle_t *pthread, const char *name, arch_os_function_return_t (*function)(void *), uint32_t stack_size, void *arg, int priority);
int arch_os_thread_delete(arch_os_thread_handle_t thread);
void arch_os_thread_finish(void);


int arch_os_mbox_new(arch_os_mbox_handle_t *pmbox, int size);
int arch_os_mbox_free(arch_os_mbox_handle_t mbox);
int arch_os_mbox_post(arch_os_mbox_handle_t mbox, void *msg, uint32_t wait_ms);
int arch_os_mbox_fetch(arch_os_mbox_handle_t mbox, void **msg, uint32_t wait_ms);

void arch_os_enter_critical(void);
void arch_os_exit_critical(void);

int arch_os_mutex_create(arch_os_mutex_handle_t *phandle);
int arch_os_mutex_delete(arch_os_mutex_handle_t handle);
int arch_os_mutex_put(arch_os_mutex_handle_t handle);
int arch_os_mutex_get(arch_os_mutex_handle_t handle, uint32_t wait_ms);

int arch_os_semaphore_create_counting(arch_os_semaphore_handle_t *pmhandle, uint32_t maxcount, uint32_t initcount);
int arch_os_semaphore_create(arch_os_semaphore_handle_t *mhandle);
int arch_os_semaphore_get(arch_os_semaphore_handle_t mhandle, uint32_t wait_ms);
int arch_os_semaphore_put(arch_os_semaphore_handle_t mhandle);
int arch_os_semaphore_delete(arch_os_semaphore_handle_t mhandle);
uint32_t arch_os_ms_elapsed(uint32_t last_ms);

#define arch_os_thread_self()               xTaskGetCurrentTaskHandle()

typedef enum os_timer_reload {
    /**
     * Create one shot timer. Timer will be in the dormant state after
     * it expires.
     */
    ARCH_OS_TIMER_ONE_SHOT,
    /**
     * Create a periodic timer. Timer will auto-reload after it expires.
     */
    ARCH_OS_TIMER_PERIODIC,
} arch_os_timer_reload_t;

/**
 * OS Timer Activate Options
 */
typedef enum os_timer_activate {
    /** Start the timer on creation. */
    ARCH_OS_TIMER_AUTO_ACTIVATE,
    /** Do not start the timer on creation. */
    ARCH_OS_TIMER_NO_ACTIVATE,
} arch_os_timer_activate_t;

int arch_os_timer_create(arch_os_timer_handle_t *ptimer, const char *name, uint32_t ms,
                         void (*call_back)(arch_os_timer_handle_t), void *context,
                         arch_os_timer_reload_t reload,
                         arch_os_timer_activate_t activate);

int arch_os_timer_activate(arch_os_timer_handle_t timer);
int arch_os_timer_change(arch_os_timer_handle_t timer, uint32_t ms);
int arch_os_timer_is_active(arch_os_timer_handle_t timer);
void *arch_os_timer_get_context(arch_os_timer_handle_t timer);
int arch_os_timer_reset(arch_os_timer_handle_t timer);
int arch_os_timer_deactivate(arch_os_timer_handle_t timer);
int arch_os_timer_delete(arch_os_timer_handle_t timer);

int arch_os_queue_create(arch_os_queue_handle_t *pqhandle, uint32_t q_len, uint32_t item_size);
int arch_os_queue_recv(arch_os_queue_handle_t qhandle, void *msg, uint32_t wait_ms);
int arch_os_queue_delete(arch_os_queue_handle_t qhandle);
int arch_os_queue_send(arch_os_queue_handle_t qhandle, const void *msg, uint32_t wait_ms);
int arch_os_queue_get_msgs_waiting(arch_os_queue_handle_t qhandle);

uint32_t arch_os_get_free_heap_size();
int arch_os_async_call(void (*cb)(void *cb_arg), void *cb_arg, int delay_ms);
void arch_os_reboot(void);
void arch_os_reboot_async(int delay_ms);

uint8_t *arch_os_mem_strstr(uint8_t *src, uint32_t src_len, uint8_t *sub, uint32_t sub_len);

#endif /* __ARCH_OS_H__ */
