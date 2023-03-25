#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "button.h"

#include "esp_timer.h"

#if 0
#define BTN_DBG ets_printf
#else
#define BTN_DBG(...)
#endif

#define IS_BUTTON_PRESSED(btn) (gpio_get_level((btn)->io_num) == (btn)->active_level)

static void button_handle_io_event(button_t *btn, btn_io_evt_t evt);
static void button_handle_btn_event(button_t *btn, btn_evt_t evt);

static void button_timer_cb(xTimerHandle tmr)
{
    button_t *btn = (button_t *)(tmr);

    button_handle_btn_event(btn, btn->tmr_btn_evt);
}

static void button_io_timer_cb(xTimerHandle tmr)
{
    button_t *btn = (button_t *)(tmr);

    button_handle_io_event(btn, btn->tmr_io_evt);
}

static IRAM_ATTR void button_disarm_timer_io(button_t *btn)
{
    btn->tmr_io_evt = BTN_IO_EVT_NONE;
    esp_timer_stop(btn->tmr_io);
}

static void button_disarm_timer(button_t *btn)
{
    btn->tmr_btn_evt = BTN_EVT_NONE;
    esp_timer_stop(btn->tmr_btn);
}

static IRAM_ATTR void button_arm_timer_io(button_t *btn, btn_io_evt_t exp_evt)
{
    button_disarm_timer_io(btn);

    switch (exp_evt) {

    case BTN_IO_EVT_PRSS:
        esp_timer_start_once(btn->tmr_io, btn->guard_press * 1000);
        btn->tmr_io_evt = exp_evt;
        break;

    case BTN_IO_EVT_RLSD:
        esp_timer_start_once(btn->tmr_io, btn->guard_release * 1000);
        btn->tmr_io_evt = exp_evt;
        break;

    case BTN_IO_EVT_RLSN:
    case BTN_IO_EVT_PRSN:
    default:
        break;
    }

}

static void button_arm_timer(button_t *btn, btn_evt_t exp_evt)
{
    button_disarm_timer(btn);

    switch (exp_evt) {

    case BTN_EVT_HOLD:
        esp_timer_start_once(btn->tmr_btn, btn->guard_hold * 1000);
        btn->tmr_btn_evt = exp_evt;
        break;

    case BTN_EVT_VLLY:
        esp_timer_start_once(btn->tmr_btn, btn->guard_volley * 1000);
        btn->tmr_btn_evt = exp_evt;
        break;

    case BTN_EVT_RLSD:
        esp_timer_start_once(btn->tmr_btn, btn->guard_click * 1000);
        btn->tmr_btn_evt = exp_evt;
        break;

    case BTN_EVT_RLSN:
    case BTN_EVT_PRSS:
    default:
        break;
    }
}

#define sta_evt(s, e)  (((s) << 8)| (e))
#define SCENARIO(s, e) (((s) << 8)| (e))

#define BUTTON_CALLBACK(btn)                    \
    btn->callback(btn->arg,                     \
                  btn->status,                  \
                  btn->click_count,             \
                  btn->volley_count);

static void button_handle_btn_event(button_t *btn, btn_evt_t evt)
{
    BTN_DBG("status-%d event-%d\r\n", btn->status, evt);

    switch (SCENARIO(btn->status, evt)) {

    case SCENARIO(BUTTON_STATE_RLSD, BTN_EVT_PRSS):
    case SCENARIO(BUTTON_STATE_RLSN, BTN_EVT_PRSS):
        /* released -> pressed */
        btn->status = BUTTON_STATE_PRSS;
        btn->click_count ++;
        BTN_DBG("button pressed, clicks=%d\r\n", btn->click_count);
        button_arm_timer(btn, BTN_EVT_HOLD);
        BTN_DBG("call callback\r\n");
        BUTTON_CALLBACK(btn);
        break;

    case SCENARIO(BUTTON_STATE_PRSS, BTN_EVT_HOLD):
        /* pressed for a long while */
        BTN_DBG("button long pressed\r\n");
        btn->status = BUTTON_STATE_HOLD;
        button_arm_timer(btn, BTN_EVT_VLLY);
        BUTTON_CALLBACK(btn);
        btn->click_count = 0;
        btn->volley_count = 0;
        break;

    case SCENARIO(BUTTON_STATE_HOLD, BTN_EVT_VLLY):
    case SCENARIO(BUTTON_STATE_VLLY, BTN_EVT_VLLY):
        /* continued press after long press */
        btn->status = BUTTON_STATE_VLLY;
        /* btn->volley_count ++; */
        if (btn->volley_count++ < 5) { // add for motor calibrate
            BTN_DBG("volleys=%d\r\n", btn->volley_count);
            button_arm_timer(btn, BTN_EVT_VLLY);
            BUTTON_CALLBACK(btn);
        }
        break;

    case SCENARIO(BUTTON_STATE_PRSS, BTN_EVT_RLSN):
        /* semi-click */
        BTN_DBG("button up lingering\r\n");
        btn->status = BUTTON_STATE_RLSN;
        BUTTON_CALLBACK(btn);
        button_arm_timer(btn, BTN_EVT_RLSD);
        break;

    case SCENARIO(BUTTON_STATE_RLSN, BTN_EVT_RLSD):
        /* click */
        BTN_DBG("button up steady\r\n");
        btn->status = BUTTON_STATE_RLSD;
        BUTTON_CALLBACK(btn);
        btn->click_count  = 0;
        btn->volley_count = 0;
        break;

    case SCENARIO(BUTTON_STATE_HOLD, BTN_EVT_RLSN):
    case SCENARIO(BUTTON_STATE_VLLY, BTN_EVT_RLSN):
        /* continued press after long press */
        BTN_DBG("button up no linger\r\n");
        btn->status = BUTTON_STATE_RLSD;
        button_disarm_timer(btn);
        BUTTON_CALLBACK(btn);
        break;

    default:
        break;
    }

}

static IRAM_ATTR void button_handle_io_event(button_t *btn, btn_io_evt_t evt)
{
    switch (sta_evt(btn->status_io, evt)) {
    case SCENARIO(BUTTON_IO_STATE_RLSD, BTN_IO_EVT_PRSN):
    case SCENARIO(BUTTON_IO_STATE_RLSN, BTN_IO_EVT_PRSN):
        btn->status_io = BUTTON_IO_STATE_PRSN;
        button_disarm_timer_io(btn);
        button_arm_timer_io(btn, BTN_IO_EVT_PRSS);
        break;

    case SCENARIO(BUTTON_IO_STATE_PRSS, BTN_IO_EVT_RLSN):
    case SCENARIO(BUTTON_IO_STATE_PRSN, BTN_IO_EVT_RLSN):
        btn->status_io = BUTTON_IO_STATE_RLSN;
        button_disarm_timer_io(btn);
        button_arm_timer_io(btn, BTN_IO_EVT_RLSD);
        break;

    case SCENARIO(BUTTON_IO_STATE_PRSN, BTN_IO_EVT_PRSS):
        BTN_DBG("button io press confirm\r\n");
        button_disarm_timer_io(btn);
        btn->status_io = BUTTON_IO_STATE_PRSS;
        button_handle_btn_event(btn, BTN_EVT_PRSS);
        break;

    case SCENARIO(BUTTON_IO_STATE_RLSN, BTN_IO_EVT_RLSD):
        BTN_DBG("button io release confirm\r\n");
        button_disarm_timer_io(btn);
        btn->status_io = BUTTON_IO_STATE_RLSD;
        button_handle_btn_event(btn, BTN_EVT_RLSN);
        break;
    }
}

// ###############################


static IRAM_ATTR void button_gpio_isr_handler(void *arg)
{
    button_t *btn = (button_t *) arg;

    if (IS_BUTTON_PRESSED(btn)) {
        button_handle_io_event(btn, BTN_IO_EVT_PRSN);
    } else {
        button_handle_io_event(btn, BTN_IO_EVT_RLSN);
    }
}

void iot_button_delete(button_t *btn)
{
    gpio_set_intr_type(btn->io_num, GPIO_INTR_DISABLE);
    gpio_isr_handler_remove(btn->io_num);

    if (btn->tmr_io) {
        esp_timer_stop(btn->tmr_io);
        esp_timer_delete(btn->tmr_io);
    }

    if (btn->tmr_btn) {
        esp_timer_stop(btn->tmr_btn);
        esp_timer_delete(btn->tmr_btn);
    }

    free(btn);
}

button_t *iot_button_create(gpio_num_t gpio_num,
                            int active_level,
                            int guard_press,
                            int guard_release,
                            int guard_hold,
                            int guard_click,
                            int guard_volley,
                            button_cb_t callback,
                            void *arg)
{
    esp_err_t err;
    button_t *btn = (button_t *) calloc(1, sizeof(button_t));

    if (!btn) {
        ets_printf("Err: failed to alloc for button\r\n");
        return NULL;
    }

    /* basic info */
    btn->active_level = active_level;
    btn->io_num = gpio_num;
    btn->guard_press   =  guard_press;
    btn->guard_release =  guard_release;
    btn->guard_hold    =  guard_hold;
    btn->guard_click   =  guard_click;
    btn->guard_volley  =  guard_volley;
    btn->callback      =  callback;
    btn->arg = arg;
    btn->status_io = BUTTON_IO_STATE_RLSD;
    btn->status    = BUTTON_STATE_RLSD;

    /* create timers */
    esp_timer_create_args_t timer_conf = { .dispatch_method = ESP_TIMER_TASK };

    timer_conf.callback = button_io_timer_cb;
    timer_conf.arg = btn;
    timer_conf.name = "btn_io_tmr";

    err = esp_timer_create(&timer_conf, &btn->tmr_io);

    if (ESP_OK != err)
        ets_printf("Err: btn_io_tmr create failed! %d\r\n", err);

    timer_conf.callback = button_timer_cb;
    timer_conf.arg = btn;
    timer_conf.name = "btn_tmr";

    err = esp_timer_create(&timer_conf, &btn->tmr_btn);

    if (ESP_OK != err)
        ets_printf("Err: btn_tmr create failed! %d\r\n", err);

    /* register isr */
    gpio_install_isr_service(0);
    gpio_config_t gpio_conf;
    gpio_conf.intr_type = GPIO_INTR_ANYEDGE;
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pin_bit_mask = (1ULL << gpio_num);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&gpio_conf);
    gpio_isr_handler_add(gpio_num, button_gpio_isr_handler, btn);

    return btn;
}
