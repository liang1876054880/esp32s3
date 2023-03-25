/*******************************************************************************
 * @file
 * @brief knob_switch APIs
 * @version 1.1.1
 * @date 2021-07-17
 ******************************************************************************/
#include "etss.h"
#include <stdio.h>
#include "etss_timer.h"
#include "ring_buffer.h"
#include "driver/gpio.h"

/************************** Define and Struct/Union Types *********************/
#define YL_TRACE_ENTER()          LOG_DEBUG("enter: %s\r\n", __FUNCTION__)

#define KEY_SERIAL_TIMEOUT        100
#define KEY_RELEASED_TIMEOUT      800
#define KEY_PRESSED_HOLD_TIMEOUT  3500   // ms
#define BUTTON_RELEASE            1
#define BUTTON_PRESSED            0

#define KNOB_SPIN_EVT_TIMEOUT     250
#define KNOB_KEY_EVT_TIMEOUT      400

#define CLOCKWISE                 1
#define ANTI_CLOCKWISE            2

#define BLE_PAIR_TIMEOUT          60000

#define ESP_INTR_FLAG_DEFAULT    0
#define GPIO_INPUT_IO_A          35
#define GPIO_INPUT_IO_B          34
#define GPIO_INPUT_IO_C          36

#define GPIO_INPUT_PIN_SEL       (1ULL << GPIO_INPUT_IO_0)

typedef struct tap_evt_s {
    uint8_t  self_gpio_num;
    uint8_t  self_gpio_val;
    uint8_t  other_gpio_num;
    uint8_t  other_gpio_val;
    uint8_t  gpioc_val;
    uint64_t time_stamp;
} tap_evt_t;

typedef struct switch_state_s {
    bool     is_spinning;
    int8_t   spin_step;     // -100 ~ 0 && 0 ~ 100
    uint8_t  click_cnt;
    uint64_t spin_start_time;
    uint32_t pressed_hold_time;
} switch_state_t;

typedef enum {
    EVT_KEY_CLICK = 1,
    EVT_KEY_HOLD  = 2,
    EVT_KEY_SPIN  = 3,
} key_evt_t;

/**
 * @brief    --  contain: spin/key..
 */
struct knob_switch_s {
    switch_state_t *switch_state;
    ring_buffer_t  *rb_tap_evt;

    etss_tmr_t     spin_handle_tmr;
    etss_tmr_t     key_handle_tmr;
    etss_tmr_t     knob_evt_clear_tmr;
    etss_tmr_t     btn_long_pressed_tmr;
};

/********************** Static Global Data section variables ******************/
struct knob_switch_s  knob_switch = {0};

bool allowed_enter_dlps = false;

static void knob_switch_evt_clear_timeout(uint32_t mdelay);
static void knob_switch_evt_clear_cb(int tid, void *data);
static void test_func_timeout(int timer_id, void *arg);
static void gpio_isr_handler(void *arg);


/************************* Static Prototype Functions *************************/
static inline int32_t _peek_evt_data(ring_buffer_t *desc, tap_evt_t *data, int size)
{
    return rb_pop_front(desc, (char *)data, sizeof(tap_evt_t) * size, 0) / sizeof(tap_evt_t);
}

static inline int32_t _pop_evt_data(ring_buffer_t *desc, tap_evt_t *data)
{
    return rb_pop_front(desc, (char *)data, sizeof(tap_evt_t), 1) / sizeof(tap_evt_t);
}

static inline int32_t _push_evt_data(ring_buffer_t *desc, tap_evt_t *data)
{
    return rb_push_back(desc, (char *)data, sizeof(tap_evt_t), 1);
}

static inline int32_t _rb_active_evt_data(ring_buffer_t *desc)
{
    uint32_t size = rb_count(desc);

    return size / sizeof(tap_evt_t);
}

static inline void _rb_data_clear(ring_buffer_t *desc)
{
    rb_reset(desc);
}


/******************************************************************************/

static void knob_gpio_init(void)
{
    /* gpio conf */
    gpio_config_t io_conf;

    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_A);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 0;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_A, gpio_isr_handler, (void *)GPIO_INPUT_IO_A);

    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_B);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 0;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_B, gpio_isr_handler, (void *)GPIO_INPUT_IO_B);

    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << GPIO_INPUT_IO_C);
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 0;
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_C, gpio_isr_handler, (void *)GPIO_INPUT_IO_C);
}

int knob_switch_init(void)
{
    YL_TRACE_ENTER();

    /* key opr evt init */
    knob_switch.switch_state = (switch_state_t *)calloc(1, sizeof(switch_state_t));

    /* ring buffer init for process key evt */
    rb_init(&knob_switch.rb_tap_evt, sizeof(tap_evt_t) * 50);

    static etss_tmr_t test_tmr;
    etss_start_tmr(etss_tmr_hdl, &test_tmr, test_func_timeout, 2000 / MS_PER_TICK);

    knob_gpio_init();

    return 0;
}

static void button_serial_evt_handler(void)
{
    knob_switch.switch_state->pressed_hold_time += KEY_SERIAL_TIMEOUT;

    // LOG_INFO("serial evt, spin step: %d, pressed time: %d ms\r\n",
    //     knob_switch.switch_state->spin_step, knob_switch.switch_state->pressed_hold_time);

    if (knob_switch.switch_state->spin_step == 0 &&
            knob_switch.switch_state->pressed_hold_time == KEY_PRESSED_HOLD_TIMEOUT) {
        LOG_INFO("long pressed %d ms \r\n", KEY_PRESSED_HOLD_TIMEOUT);
    }

    if ((knob_switch.switch_state->pressed_hold_time / 1000)
            && (!(knob_switch.switch_state->pressed_hold_time % 1000))) {

        LOG_INFO("switch holding %d ms \r\n", knob_switch.switch_state->pressed_hold_time);
    }

    knob_switch_evt_clear_timeout(KEY_RELEASED_TIMEOUT);
}

static void knob_switch_key_evt_timeout(int tid, void *data)
{
    LOG_INFO("click cnt: %d \r\n", knob_switch.switch_state->click_cnt);

    if (knob_switch.switch_state->spin_step == 0) {
        // publish
        LOG_INFO("pub click cnt: %d \r\n", knob_switch.switch_state->click_cnt);

        etss_peripheral_req_t peripheral_req;
        peripheral_req.event = PERIPHERAL_EVT_LCD;
        peripheral_req.type  = PERIPHERAL_TYPE_KNOB;
        peripheral_req.data[0] = 8; // todo
        peripheral_req.data[1] = knob_switch.switch_state->click_cnt;

        ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                          sizeof(etss_peripheral_req_t), NULL, NULL);

    } else {
        LOG_ERROR("ignore click, spin_step: %d, switch click evt cnt: %d \r\n",
                  knob_switch.switch_state->spin_step, knob_switch.switch_state->click_cnt);
    }

    /* clear click and spin evt */
    knob_switch.switch_state->spin_step = 0;
    knob_switch.switch_state->click_cnt = 0;
}

static void button_released_evt_handler(void)
{
    uint32_t timeout = knob_switch.switch_state->pressed_hold_time;

    /* clear pressed_hold_time and spinning evt */
    knob_switch.switch_state->pressed_hold_time = 0;
    knob_switch.switch_state->is_spinning       = false;
    knob_switch.switch_state->spin_start_time   = 0;

    if (timeout >= KEY_RELEASED_TIMEOUT) {
        LOG_ERROR("ignore click, pressed timeout: %d ms\r\n", timeout);
        return;
    }

    /* click evt */
    knob_switch.switch_state->click_cnt++;
    /* LOG_DEBUG("switch click cnt: %d \r\n", knob_switch.switch_state->click_cnt); */

    if (etss_timer_is_running(&knob_switch.key_handle_tmr)) {
        etss_del_timer(&knob_switch.key_handle_tmr);
    }

    /* click evt timeout */
    etss_start_tmr(etss_tmr_hdl, &knob_switch.key_handle_tmr,
                   knob_switch_key_evt_timeout, (KNOB_KEY_EVT_TIMEOUT / MS_PER_TICK));

}

static void button_pressed_timeout_cb(int id, void *data)
{
    /* pressed hold check */
    uint32_t gpio_val =  gpio_get_level(GPIO_INPUT_IO_C);
    if (gpio_val != BUTTON_PRESSED) {
        return;
    }

    /* pressed hold check */
    if (etss_timer_is_running(&knob_switch.btn_long_pressed_tmr)) {
        etss_del_timer(&knob_switch.btn_long_pressed_tmr);
    }

    etss_start_tmr(etss_tmr_hdl, &knob_switch.btn_long_pressed_tmr,
                   button_pressed_timeout_cb, (KEY_SERIAL_TIMEOUT / MS_PER_TICK));

    /* serial evt */
    button_serial_evt_handler();
}

static int64_t get_time_100ns(void)
{
    return esp_timer_get_time() / 100;
}

static void knob_spin_evt_handle(bool knob_is_pressed)
{
    uint32_t spin_time = get_time_100ns() - knob_switch.switch_state->spin_start_time;

    if (abs(knob_switch.switch_state->spin_step) < 2 && spin_time < 2000) {

        LOG_ERROR("ignore spin, knob spin step: %d, spin time: %d ms\r\n",
                  knob_switch.switch_state->spin_step, spin_time);

        return;
    }

    LOG_INFO("-------------> [%s], step: %d\r\n", knob_is_pressed ? "pressed" : "released",
             knob_switch.switch_state->spin_step);

    etss_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_KNOB;
    peripheral_req.data[0] = knob_is_pressed ? 9 : 10; // todo
    peripheral_req.data[1] = knob_switch.switch_state->spin_step;

    ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(etss_peripheral_req_t), NULL, NULL);
}

static void knob_switch_spin_evt_timeout(int tid, void *data)
{
    int16_t  spin_step          = 0;
    uint8_t  spin_direct        = 0;
    uint32_t pressed_cnt        = 0;
    bool     is_pressed         = false;
    uint32_t last_time          = 0;
    uint8_t  time_interval      = 0;
    uint32_t interval_total_cnt = 0;
    uint16_t acc_speed          = 0;
    uint8_t  invalid_step_cnt   = 0;
    uint8_t  invalid_spin_step  = 0;
    uint32_t direct_off_val     = 0;

    uint16_t cnt = _rb_active_evt_data(knob_switch.rb_tap_evt);
    if (cnt < 3) {
        LOG_ERROR("ignore, spin step: %d\r\n", cnt);

        _rb_data_clear(knob_switch.rb_tap_evt);
        return;
    }

    /* 1, check direction  and  accelerated speed for spin */
    tap_evt_t *peek_spin_data = (tap_evt_t *)calloc(cnt, sizeof(tap_evt_t));
    _peek_evt_data(knob_switch.rb_tap_evt, peek_spin_data, cnt);


    /* (a), direction */
    for (int i = 0; i < 3; i++) {
        direct_off_val |= (peek_spin_data[i].self_gpio_num == GPIO_INPUT_IO_B) ?
                          (peek_spin_data[i].self_gpio_val << 4 | peek_spin_data[i].other_gpio_val) :
                          (peek_spin_data[i].other_gpio_val << 4 | peek_spin_data[i].self_gpio_val);

        if (i < 2) {
            direct_off_val = (direct_off_val << 8);
        }
    }

    LOG_INFO("---direct_off_val : 0x%06x \r\n", direct_off_val);

    switch (direct_off_val) {
    case 0x100001:
    case 0x100000:
    case 0x100010:
    case 0x101011:
    case 0x101000:
    case 0x101001:
    case 0x101110:
    case 0x011110:
    case 0x000111:
        spin_direct = CLOCKWISE;
        break;
    case 0x011101:
    case 0x010100:
    case 0x001011:
    case 0x101101:
    case 0x010010:
    case 0x010000:
    case 0x010110:
    case 0x010001:
    case 0x010111:
        spin_direct = ANTI_CLOCKWISE;
        break;
    default:
        break;
    }

    /* (b), pressed and speed */
    for (int i = 0; i < cnt; i++) {
        time_interval = peek_spin_data[i].time_stamp - last_time;
        last_time = peek_spin_data[i].time_stamp;

        /* LOG_DEBUG("--- [%d] s_gpio[%d]: %d, o_gpio[%d]: %d c_gpio: %d, stamp: %lld, inter: %d\r\n", i, */
        /*      peek_spin_data[i].self_gpio_num, peek_spin_data[i].self_gpio_val, */
        /*      peek_spin_data[i].other_gpio_num, peek_spin_data[i].other_gpio_val, */
        /*      peek_spin_data[i].gpioc_val, peek_spin_data[i].time_stamp, time_interval); */

        if (time_interval >= 2 && (i > 0)) {
            interval_total_cnt += time_interval;
        } else {
            invalid_step_cnt++;
            if (peek_spin_data[i].self_gpio_num == GPIO_INPUT_IO_B) { // KNOB_GPIOB_PIN
                invalid_spin_step++;
            }
        }

        if (peek_spin_data[i].gpioc_val != BUTTON_RELEASE) {
            pressed_cnt++;
        }
    }

    free(peek_spin_data);

    /* acc_speed smaller, spin faster, offset = acc_speed */
    LOG_DEBUG("cnt: %d, invalid_step_cnt: %d,  interval_total_cnt: %d\r\n",
              cnt, invalid_step_cnt, interval_total_cnt);

    acc_speed = (100.0 * interval_total_cnt / (cnt - invalid_step_cnt));

    is_pressed = (cnt > 5) ?
                 ((pressed_cnt >= cnt - 2) ? true : false) : (pressed_cnt > 0 ? true : false);

    LOG_DEBUG("is pressed: %d, acc_speed: %d \r\n", is_pressed, acc_speed);

    /* 2, determine the rotation scale of the knob */
    tap_evt_t evt_data = {0};
    for (int i = 0; i < cnt; i++) {
        _pop_evt_data(knob_switch.rb_tap_evt, &evt_data);

        if (is_pressed != (BUTTON_RELEASE != evt_data.gpioc_val)) { // fliter pressed/released, err gpio irq
            continue;
        }

        if (evt_data.self_gpio_num == GPIO_INPUT_IO_B) { // KNOB_GPIOB_PIN
            spin_step++;
            /* use the first few interruptions to determine the direction of rotation */
        }
    }

    LOG_DEBUG("1 spin_step: %d, invalid_spin_step: %d\r\n", spin_step, invalid_spin_step);

    /* spin_step = (1500.0 / acc_speed) * spin_step + 1; // test val */
    /* spin_step = (spin_step > 100) ? 100 : spin_step; */

    spin_step = (spin_step - invalid_spin_step) / 2;

    spin_step = (spin_step == 0) ? 1 : spin_step;

    LOG_DEBUG("-----spin_step: %d\r\n", spin_step);

    switch (spin_direct) {
    case CLOCKWISE:
        knob_switch.switch_state->spin_step = spin_step;
        knob_spin_evt_handle(is_pressed);
        break;
    case ANTI_CLOCKWISE:
        knob_switch.switch_state->spin_step = 0 - spin_step;
        knob_spin_evt_handle(is_pressed);
        break;
    default:
        LOG_ERROR("-----INVALID spin_direct! return\r\n");
        break;
    }
}

static void knob_switch_evt_clear_cb(int tid, void *data)
{
    etss_del_timer(&knob_switch.spin_handle_tmr);
    etss_del_timer(&knob_switch.key_handle_tmr);

    /* LOG_DEBUG("knob switch evt clear\r\n"); */
    knob_switch.switch_state->spin_step       = 0;
    knob_switch.switch_state->spin_start_time = 0;
    knob_switch.switch_state->is_spinning     = false;

    /* stop tmr, enter dlps */
    allowed_enter_dlps = true;
}


static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;

    etss_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_KNOB;
    peripheral_req.type  = PERIPHERAL_TYPE_KNOB;
    peripheral_req.data[0] = gpio_num;
    peripheral_req.data[1] = gpio_get_level(GPIO_INPUT_IO_A);
    peripheral_req.data[2] = gpio_get_level(GPIO_INPUT_IO_B);
    peripheral_req.data[3] = gpio_get_level(GPIO_INPUT_IO_C);
    peripheral_req.data[4] = get_time_100ns();

    ETSS_POST_REQUEST(ETSS_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(etss_peripheral_req_t), NULL, NULL);
}

void knob_evt_handler(uint32_t gpio_num, uint8_t gpioA_val,
                      uint8_t gpioB_val, uint8_t gpioC_val, int time_stamp)
{
    /* LOG_INFO("gpio_num[%d], A[%d] B[%d] C[%d], stamp: %d\r\n", */
    /*          gpio_num, gpioA_val, gpioB_val, gpioC_val, time_stamp); */

    switch (gpio_num) {
    case GPIO_INPUT_IO_A: // knob spin evt
    case GPIO_INPUT_IO_B: {
        tap_evt_t tap_evt = {0};
        tap_evt.self_gpio_num  = gpio_num;
        tap_evt.self_gpio_val  = (gpio_num == GPIO_INPUT_IO_A) ? gpioA_val : gpioB_val;
        tap_evt.other_gpio_num = (gpio_num == GPIO_INPUT_IO_A) ? GPIO_INPUT_IO_B : GPIO_INPUT_IO_A;
        tap_evt.other_gpio_val = (gpio_num == GPIO_INPUT_IO_A) ? gpioB_val : gpioA_val;

        tap_evt.gpioc_val  = gpioC_val;
        tap_evt.time_stamp = time_stamp;

        _push_evt_data(knob_switch.rb_tap_evt, &tap_evt);

        /* is spinning */
        if (!knob_switch.switch_state->is_spinning) {
            knob_switch.switch_state->is_spinning = true;
            knob_switch.switch_state->spin_start_time = get_time_100ns(); // arch_os_ms_now();
        }

        /* keep spin events triggered continuously */
        if (!etss_timer_is_running(&knob_switch.spin_handle_tmr)) {
            etss_start_tmr(etss_tmr_hdl,
                           &knob_switch.spin_handle_tmr,
                           knob_switch_spin_evt_timeout,
                           (KNOB_SPIN_EVT_TIMEOUT / MS_PER_TICK));
        }
    } break;
    case GPIO_INPUT_IO_C:  // switch button click evt
        if (gpioC_val == BUTTON_PRESSED) {
            LOG_INFO("gpio36 evt pressed");

            if (etss_timer_is_running(&knob_switch.btn_long_pressed_tmr)) {
                etss_del_timer(&knob_switch.btn_long_pressed_tmr);
            }

            /* pressed hold check after KEY_SERIAL_TIMEOUT ms */
            etss_start_tmr(etss_tmr_hdl, &knob_switch.btn_long_pressed_tmr,
                           button_pressed_timeout_cb, (KEY_SERIAL_TIMEOUT / MS_PER_TICK));
        } else {
            button_released_evt_handler();
        }
        break;
    default:
        break;
    }

    knob_switch_evt_clear_timeout(KEY_RELEASED_TIMEOUT);
}

static void knob_switch_evt_clear_timeout(uint32_t mdelay)
{

    if (etss_timer_is_running(&knob_switch.knob_evt_clear_tmr)) {
        etss_del_timer(&knob_switch.knob_evt_clear_tmr);
    }

    etss_start_tmr(etss_tmr_hdl, &knob_switch.knob_evt_clear_tmr,
                   knob_switch_evt_clear_cb, (mdelay / MS_PER_TICK));
}

static void test_func_timeout(int timer_id, void *arg)
{
    static int cnt = 0;

    cnt++;
    LOG_DEBUG("test func, timeout cnt: %d\r\n", cnt);
}

