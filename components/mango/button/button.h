#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "driver/gpio.h"
#include "freertos/portmacro.h"

typedef enum {
    BUTTON_IO_STATE_PRSN    = 1,
    BUTTON_IO_STATE_PRSS    = 2,
    BUTTON_IO_STATE_RLSN    = 4,
    BUTTON_IO_STATE_RLSD    = 5,
} button_io_status_t;

typedef enum {
    BTN_IO_EVT_NONE,
    BTN_IO_EVT_PRSN, /* gpio level changed to active */
    BTN_IO_EVT_PRSS, /* gpio been active for a while */
    BTN_IO_EVT_RLSN, /* gpio level changed to inactive */
    BTN_IO_EVT_RLSD, /* gpio been inactive for a while */
} btn_io_evt_t;


typedef enum {
    BUTTON_STATE_PRSS    = 2,   /* press */
    BUTTON_STATE_HOLD    = 3,   /* hold */
    BUTTON_STATE_RLSN    = 4,   /* release */
    BUTTON_STATE_RLSD    = 5,   /* hold release */
    BUTTON_STATE_VLLY    = 6,   /* period event, with use of hold */
} button_status_t;

typedef enum {
    BTN_EVT_NONE,
    BTN_EVT_PRSS, /* debounced press */
    BTN_EVT_RLSN, /* debounced release */
    BTN_EVT_RLSD, /* release confirmed */
    BTN_EVT_HOLD, /* long press */
    BTN_EVT_VLLY, /* volley */
} btn_evt_t;

typedef void (*button_cb_t)(void *arg, button_status_t status, int clicks, int volleys);

typedef struct {
    /* input fields */
    uint8_t            io_num;
    uint8_t            active_level;
    int                click_max; // not handled yet
    button_cb_t        callback;  /* released but not confirmed */
    void              *arg;
    int                guard_press;      /* debounce timer for press event */
    int                guard_release;    /* debounce timer for release event */
    int                guard_hold;       /* debounce timer to trgger long press */
    int                guard_click;      /* a short lag before trigger click event
                                          * after release event (distinguish
                                          * clicks and multiple clicks)
                                          */
    int                guard_volley;      /* interval between voley events (after long press) */
    /* timers */
    esp_timer_handle_t tmr_io;            /* release debounce, press debounce*/
    esp_timer_handle_t tmr_btn;           /* long press, voley, click */
    btn_io_evt_t       tmr_io_evt;
    btn_evt_t          tmr_btn_evt;
    /* internal status */
    button_io_status_t status_io;
    button_status_t    status;
    /* output fields */
    int                click_count;
    int                volley_count;
    uint8_t            pressed_at_init;
} button_t;

extern button_t *iot_button_create(gpio_num_t gpio_num,
                                   int active_level,
                                   int guard_press,
                                   int guard_release,
                                   int guard_hold,
                                   int guard_click,
                                   int guard_volley,
                                   button_cb_t callback,
                                   void *arg);

extern void iot_button_delete(button_t *btn);

#endif // _BUTTON_H_
