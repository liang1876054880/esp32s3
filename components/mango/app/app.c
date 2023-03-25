#include "bioc_timer.h"
#include "bioc.h"
#include "button.h"
#include "lcd.h"
#include "font.h"
#include "arch_os.h"
#include "lan_ctrl.h"
#include "esp_log.h"
#include "motor.h"

/* #include "spi_slave.h" */
/* #include "bioc_spec.h" */

#if 0
#define DRS_DBG ets_printf
#else
#define DRS_DBG(...)
#endif

#define GUARD_PRESS     20   // filer jitters caused by electronic static discharge
#define GUARD_RELEASE   60   // filter jitters while releasing button
#define GUARD_HOLD      500
#define GUARD_CLICK     300
#define GUARD_VOLLEY    10
#define GUARD_RESET     3000

#define BUTTON_SWITCH_IO_NUM    0
#define BUTTON_SWITCH2_IO_NUM   39
#define BUTTON_SWITCH3_IO_NUM   34
#define BUTTON_SWITCH4_IO_NUM   35
#define BUTTON_SWITCH5_IO_NUM   32


/* #define LED_BLINK_IO_NUM   32 */
/* #define PWR_IO_NUM         33 */
/* #define PWR_IO_NUM1        22 */

#define BUTTON_ACTIVE_LEVEL     0

#define BUTTON_PRESS        1
#define BUTTON_CLICK        2
#define BUTTON_HOLD         3


extern void motor_init(void);

extern void nvs_storage_app_init(void);

extern esp_err_t set_motor_calibration_val(void);


static void button_init(void);
static int  dev_prod_init(void);
static void button_event_handler(void *arg, button_status_t status, int clicks, int volleys);

static void power_on_life_cb(int tid, void *arg);
static void led_init(void);

bioc_prod_init_func_t bioc_prod_init_tbl[] = {
    dev_prod_init,
    NULL,    /* !! NULL MUST be here as a sentinal  */
};

uint8_t btn1_id = 1;
uint8_t btn2_id = 2;
uint8_t btn3_id = 3;
uint8_t btn4_id = 4;
uint8_t btn5_id = 5;

uint8_t limiter1_id = 6;
uint8_t limiter2_id = 7;
uint8_t limiter3_id = 8;
uint8_t limiter4_id = 9;

extern void uart_app_init(void);
static const char *TAG = "app";

static int dev_prod_init(void)
{
    LOG_INFO("dev init\r\n");

    /* button_init(); */

    /* nvs_storage_app_init(); */

    uart_app_init();

    /* motor_init(); */

    /* led_init(); */

    /* lan_ctrl_init(); */

    bioc_tmr_t *power_on_tmr = (bioc_tmr_t *)calloc(1, sizeof(bioc_tmr_t));

    bioc_start_tmr_with_data(bioc_tmr_hdl, power_on_tmr, power_on_life_cb,
                             1000 / MS_PER_TICK, (void *)power_on_tmr);

    return 0;
}

static void power_on_life_cb(int tid, void *arg)
{
    bioc_tmr_t *power_on_tmr = (bioc_tmr_t *)arg;

    LOG_INFO("test timer, cur time: %d .....\r\n", arch_os_ms_now());

    static int cnt = 0;
    cnt++;

    static bool _set_lcd_back_led_flag = 1;

    if (_set_lcd_back_led_flag) {

        _set_lcd_back_led_flag = 0;

        /* LCD_BLK_Set(); */
    }

#if 0
    if (cnt / 10 && (!(cnt % 10))) { // 2s
        gpio_set_level(PWR_IO_NUM, 1);
    } else {
        gpio_set_level(PWR_IO_NUM, 0);
    }
#endif

    /* bioc_task_timer_update(); */
    bioc_peripheral_req_t peripheral_req;
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type  = PERIPHERAL_TYPE_TIME;

    BIOC_POST_REQUEST(BIOC_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(bioc_peripheral_req_t), NULL, NULL);

    bioc_start_tmr_with_data(bioc_tmr_hdl,
                             power_on_tmr,
                             power_on_life_cb,
                             5000 / MS_PER_TICK, (void *)arg);
}

static void led_init(void)
{
    /* gpio_config_t io_conf; */
    /* io_conf.intr_type = GPIO_INTR_DISABLE; */
    /* io_conf.mode = GPIO_MODE_OUTPUT; */
    /* io_conf.pin_bit_mask = (1ULL << LED_BLINK_IO_NUM); */
    /* io_conf.pull_down_en = 0; */
    /* io_conf.pull_up_en = 1; */
    /* gpio_config(&io_conf); */

    /* io_conf.intr_type = GPIO_INTR_DISABLE; */
    /* io_conf.pin_bit_mask = (1ULL << PWR_IO_NUM); */
    /* io_conf.mode = GPIO_MODE_OUTPUT; */
    /* io_conf.pull_down_en = 0; */
    /* io_conf.pull_up_en = 1; */
    /* gpio_config(&io_conf); */

    /* io_conf.intr_type = GPIO_INTR_DISABLE; */
    /* io_conf.pin_bit_mask = (1ULL << PWR_IO_NUM1); */
    /* io_conf.mode = GPIO_MODE_OUTPUT; */
    /* io_conf.pull_down_en = 0; */
    /* io_conf.pull_up_en = 1; */
    /* gpio_config(&io_conf); */

}

static void button_init(void)
{
    /* gpio init , must do it first */
    gpio_config_t io_conf;
    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_SWITCH_IO_NUM);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  BUTTON_SWITCH2_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  BUTTON_SWITCH3_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  BUTTON_SWITCH4_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  BUTTON_SWITCH5_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  SWITCH_LIMITER1_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    io_conf.intr_type    = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL <<  SWITCH_LIMITER2_IO_NUM);
    io_conf.mode         = GPIO_MODE_DEF_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en   = 1;
    gpio_config(&io_conf);

    LOG_INFO("1 get level:%d\n", gpio_get_level(BUTTON_SWITCH_IO_NUM));
    LOG_INFO("2 get level:%d\n", gpio_get_level(BUTTON_SWITCH2_IO_NUM));
    LOG_INFO("3 get level:%d\n", gpio_get_level(BUTTON_SWITCH3_IO_NUM));
    LOG_INFO("4 get level:%d\n", gpio_get_level(BUTTON_SWITCH4_IO_NUM));
    LOG_INFO("5 get level:%d\n", gpio_get_level(BUTTON_SWITCH5_IO_NUM));
    LOG_INFO("6 get level:%d\n", gpio_get_level(SWITCH_LIMITER1_IO_NUM));
    LOG_INFO("7 get level:%d\n", gpio_get_level(SWITCH_LIMITER2_IO_NUM));

    iot_button_create(BUTTON_SWITCH_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &btn1_id);

    iot_button_create(BUTTON_SWITCH2_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &btn2_id);

    iot_button_create(BUTTON_SWITCH3_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &btn3_id);

    iot_button_create(BUTTON_SWITCH4_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &btn4_id);

    iot_button_create(BUTTON_SWITCH5_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &btn5_id);

    iot_button_create(SWITCH_LIMITER1_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &limiter1_id);

    iot_button_create(SWITCH_LIMITER2_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &limiter2_id);

    iot_button_create(SWITCH_LIMITER3_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &limiter3_id);

    iot_button_create(SWITCH_LIMITER4_IO_NUM, BUTTON_ACTIVE_LEVEL,
                      GUARD_PRESS, GUARD_RELEASE, GUARD_HOLD, GUARD_CLICK,
                      GUARD_VOLLEY, button_event_handler, &limiter4_id);

}

static void button_event_handler(void *arg, button_status_t status, int clicks, int volleys)
{
    uint8_t key_num = *(uint8_t *)arg;

    DRS_DBG("button[%d] status: %d, clicks: %d, volleys: %d\r\n",
            key_num, status, clicks, volleys);

    bioc_peripheral_req_t peripheral_req = {0};

    switch (status) {
    case BUTTON_STATE_HOLD:
    case BUTTON_STATE_VLLY:
        DRS_DBG("post volley event\r\n");
        peripheral_req.event   = PERIPHERAL_EVT_KEY;
        peripheral_req.type    = BUTTON_HOLD;
        peripheral_req.data[0] = key_num;
        peripheral_req.data[1] = (char)volleys;
        peripheral_req.data[2] = (char)(volleys >> 8);
        peripheral_req.data[3] = (char)(volleys >> 16);
        peripheral_req.data[4] = (char)(volleys >> 24);
        goto post_event;

    case BUTTON_STATE_RLSN:
        break;
    case BUTTON_STATE_RLSD:
        /* emit event early in abscence of double click support */
        peripheral_req.event   = PERIPHERAL_EVT_KEY;
        peripheral_req.type    = BUTTON_CLICK;
        peripheral_req.data[0] = key_num;
        peripheral_req.data[1] = clicks;
        DRS_DBG("post click num: %d\r\n", clicks);
        goto post_event;

    case BUTTON_STATE_PRSS:
        peripheral_req.event   = PERIPHERAL_EVT_KEY;
        peripheral_req.type    = BUTTON_PRESS;
        peripheral_req.data[0] = key_num;
        goto post_event;
    default:
        break;
    }

    goto done;

post_event:
    BIOC_POST_REQUEST(BIOC_PERIPHERAL_EVT, &peripheral_req, sizeof(bioc_peripheral_req_t), NULL, NULL);

done:
    return;
}

#if 0
void button_evt_process(uint8_t key_evt, uint8_t key_num, uint8_t key_click)
{
    static float t = 0;
    uint8_t cmd[4] = {0, 0, 0, 0};
    char buffer[24] = {0};

    LOG_INFO("key_evt[%d], [%d] click: %d\r\n", key_evt, key_num, key_click);

    switch (key_evt) {
    case BUTTON_PRESS:
        gpio_set_level(LED_BLINK_IO_NUM, 1);

        break;
    case BUTTON_CLICK:
        gpio_set_level(LED_BLINK_IO_NUM, 0);
        cmd[0] = key_num;
        cmd[1] = key_click;
        sprintf(buffer, "[%d] click: %d   ", key_num, key_click);

        /* LCD_ShowString(2, 120, (const uint8_t *)buffer, YELLOW, BLACK, 16, 0); */
        /* send_data_to_CCU(TYPE_CMD_SEND, cmd, 4); */

        if (key_num == 1) {         // key1
            if (key_click == 2) {
            }

            if (key_click == 1) {
                PageEventTransmit(&key_num, 1);
            }

        } else if (key_num == 2) {  // key2
            if (key_click == 2) {
                /* LCD_Fill(30, 18, 116, 50, BLACK); */
            }
            /*  */
            /* if (key_click == 1) { */
            /*     LCD_BLK_Set(); */
            /* } */
        }

        if (key_click == 3) {
            /* LCD_Fill(26, 18, 214, 90, BLACK); */
        }

        break;
    case BUTTON_HOLD:
        /* LCD_ShowFloatNum1(136, 120, t, 6, GRAYBLUE, BLACK, 16); */
        t += 0.10;
        break;

    default:
        break;
    };
}

bioc_errno_t bioc_proc_peripheral(const bioc_peripheral_req_t *req)
{
    DRS_DBG("enter : %s\r\n", __FUNCTION__);
    DRS_DBG("evt: %d\r\n", req->event);

    static float tm = 0;
    char buffer[18] = {0};

    switch (req->event) {
    case PERIPHERAL_EVT_KNOB:
        break;
    case PERIPHERAL_EVT_KEY:
        button_evt_process(req->type, req->data[0], req->data[1]);
        break;
    case PERIPHERAL_EVT_LCD:
        break;
    }

    return BIOC_ERR_OK;
}
#endif

bioc_errno_t bioc_proc_temp_data(const temp_data_t *data)
{

    LOG_INFO("req, cur time: %d, temp: <%.3f, %.3f, %.3f, %.3f, %.3f, %.3f> \r\n",
             arch_os_ms_now(), data->ch0_temp, data->ch1_temp,
             data->ch2_temp, data->ch3_temp,
             data->ch4_temp, data->ch5_temp);

    bioc_peripheral_req_t peripheral_req = {0};
    peripheral_req.event = PERIPHERAL_EVT_LCD;
    peripheral_req.type = PERIPHERAL_TYPE_TEMP;

    memcpy(peripheral_req.data, data, 48);

    BIOC_POST_REQUEST(BIOC_PERIPHERAL_EVT, &peripheral_req,
                      sizeof(bioc_peripheral_req_t), NULL, NULL);

    /* report data to net */
    bioc_try_report_temp_data(data);

    return BIOC_ERR_OK;
}

