#include "stepper.h"
#include "esp_log.h"
#include "motor.h"
#include "bioc_timer.h"
#include "bioc.h"

#define MI_STEP  MICROSTEP_2

#define M1_STOPER_BEGIN  7
#define M1_STOPER_END    6

#define M2_STOPER_BEGIN  8
#define M2_STOPER_END    9

#define PRESS        1
#define CLICK        2
#define HOLD         3

#define MOTOR_IN_MIDDLE 0
#define MOTOR_IN_END    1
#define MOTOR_IN_BEGIN  2


/* 通过获取限位器的状态，判断电机当前位置, 为上电校准做准备，防止过冲 */
/* MOTOR_CALI_VAL(M1_STOPPER_BEGIN_VAL, M1_STOPPER_END_VAL) */
/* stopper_begin_val stopper_end_val    val */
/* 0                       0              0 */ // 当前位置 中间, 向上运行
/* 0                       1              1 */ // 当前位置 底部, 向上运行
/* 1                       0              2 */ // 当前位置 顶部, 向下运行
/* 1                       1              3 */
#define MOTOR_CALI_VAL(motor_begin, motor_end)  ((motor_begin << 1) | (motor_end))

#define M1_STOPPER_BEGIN_VAL !gpio_get_level(SWITCH_LIMITER2_IO_NUM) // 检测到时，电平为0
#define M1_STOPPER_END_VAL   !gpio_get_level(SWITCH_LIMITER1_IO_NUM)

#define M2_STOPPER_BEGIN_VAL !gpio_get_level(SWITCH_LIMITER3_IO_NUM)
#define M2_STOPPER_END_VAL   !gpio_get_level(SWITCH_LIMITER4_IO_NUM)

extern esp_err_t set_motor_state_val(void);
extern esp_err_t get_motor_state_val(void);

static void start_motor_calibration(void);

static motor_state_t get_motor_state(void);

dev_stepper_t motor1 =  {
    .ctrl = {
        .stepInterval = 40000,
        .rest         = 0,
        .stepCnt      = 0,
        .stepsToGo    = 0,
        .speed        = 100,
        .acc          = 100,
        .enOffTime    = 10000L,
        .status       = DISABLED,
        .dir          = CW,
        .homed        = false,
        .runInfinite  = false,
    },

    .conf = {
        .step_p      = 26,
        .dir_p       = 25,
        .en_p        = 33,
        .timer_group = TIMER_GROUP_0,
        .timer_idx   = TIMER_0,
        .miStep      = MI_STEP,
    },
};

dev_stepper_t motor2 =  {
    .ctrl = {
        .stepInterval = 40000,
        .rest         = 0,
        .stepCnt      = 0,
        .stepsToGo    = 0,
        .speed        = 100,
        .acc          = 100,
        .enOffTime    = 10000L,
        .status       = DISABLED,
        .dir          = CW,
        .homed        = false,
        .runInfinite  = false,

    },

    .conf = {
        .step_p      = 21,
        .dir_p       = 22,
        .en_p        = 27,
        .timer_group = TIMER_GROUP_1,
        .timer_idx   = TIMER_0,
        .miStep      = MI_STEP,
    },

};

static const char *TAG = "motor";
static const int motor_calibrate_step_max = 200 * MI_STEP * 999;//1.8° * 200 一圈,  999圈,

static int m1_stopper_begin_detected = 0; // 上限位 stopper_end_detected
static int m1_stopper_end_detected   = 0; // 下限位
static int m2_stopper_begin_detected = 0; // 上限位
static int m2_stopper_end_detected   = 0; // 下限位

static bioc_tmr_t motor_calibrate_tmr = {0};
static bioc_tmr_t motor_open_tmr      = {0};
static bioc_tmr_t motor_close_tmr     = {0};
static bioc_tmr_t motor_running_tmr   = {0};

static bioc_tmr_t motor_startup_tmr   = {0};

extern motor_calibrate_t motor_state;

void motor_stopper_event_process(int stopper_id, int event, int data)
{
    printf("evt stopper: %d, evt: %d , hold_time: %d\r\n", stopper_id, event, data);

    if (event == HOLD && getState(&motor1) == IDLE && (getState(&motor2) == IDLE)) {
        printf("return, idle\r\n");
        return;
    }

    switch (stopper_id) {
    case M1_STOPER_BEGIN:
        if ((event == PRESS && !motor_state.m1_calibrated) /*need calibration*/ ||
                ((event == HOLD && data > 1) && getState(&motor1) != IDLE)) { /*to avoid hit*/
            stop(&motor1);
            resetAbsolute(&motor1);
            m1_stopper_begin_detected = 1;
            motor_state.m1_begin_position = getPosition(&motor1);

            printf("-->evt: %d(1press, 3hold), motor_state.m1_calibrated: %d\r\n", event, motor_state.m1_calibrated);
            printf("M1_STOPER_BEGIN, motor_state.m1_begin_position: %d \r\n", motor_state.m1_begin_position);
        }

        break;
    case M1_STOPER_END:
        if ((event == PRESS && !motor_state.m1_calibrated) /*need calibration*/ ||
                ((event == HOLD && data > 1) && getState(&motor1) != IDLE)) { /*to avoid hit*/
            stop(&motor1);
            m1_stopper_end_detected = 1;
            motor_state.m1_end_position = getPosition(&motor1);
            printf("-->evt: %d(1press, 3hold), motor_state.m1_calibrated: %d\r\n", event, motor_state.m1_calibrated);
            printf("M1_STOPER_END, motor_state.m1_end_position: %d \r\n", motor_state.m1_end_position);
        }

        break;
    case M2_STOPER_BEGIN:
        if ((event == PRESS && !motor_state.m2_calibrated) /*need calibration*/ ||
                ((event == HOLD && data > 1) && getState(&motor2) != IDLE)) { /*to avoid hit*/
            stop(&motor2);
            resetAbsolute(&motor2);
            m2_stopper_begin_detected = 1;
            motor_state.m2_begin_position = getPosition(&motor2);

            printf("-->evt: %d(1press, 3hold), motor_state.m2_calibrated: %d\r\n", event, motor_state.m2_calibrated);
            printf("M2_STOPER_BEGIN, motor_state.m2_begin_position: %d \r\n", motor_state.m2_begin_position);
        }

        break;
    case M2_STOPER_END:
        if ((event == PRESS && !motor_state.m2_calibrated) /*need calibration*/ ||
                ((event == HOLD && data > 1) && getState(&motor2) != IDLE)) { /*to avoid hit*/
            stop(&motor2);
            m2_stopper_end_detected = 1;
            motor_state.m2_end_position = getPosition(&motor2);
            printf("-->evt: %d(1press, 3hold), motor_state.m2_calibrated: %d\r\n", event, motor_state.m2_calibrated);
            printf("M2_STOPER_END, motor_state.m2_end_position: %d \r\n", motor_state.m2_end_position);
        }
        break;
    default:
        break;
    }

    if (m1_stopper_begin_detected && m1_stopper_end_detected) {
        motor_state.m1_calibrated = 1;
    }

    if (m2_stopper_begin_detected && m2_stopper_end_detected) {
        motor_state.m2_calibrated = 1;
    }

    /* set_motor_state_val(); */
}

static void motor_running_process_cb(int tid, void *arg)
{
    int phase = *(int *)arg;

    if (phase == 0 && getState(&motor2) == IDLE) {

        motor_state.m_current_state = MOTOR_RUNNING;

        /* set_motor_state_val(); */
        return;
    }

    bioc_start_tmr_with_data(bioc_tmr_hdl,
                             &motor_open_tmr,
                             motor_running_process_cb,
                             100 / MS_PER_TICK, arg);
}

/* motor2  motor1   state */
/* end     start    runing */
/* start   start    close */
/* start   end      open */

int run_motor(void)
{
    motor_state_t state = get_motor_state();

    if ((getState(&motor1) != IDLE || (getState(&motor2) != IDLE))) {
        printf("motor running, error\r\n");
        return -1;
    }

    if (state == MOTOR_NONE) {
        printf(" MOTOR_NONE, error\r\n");
        return -1;
    }

    if (state == MOTOR_OPEN) {
        printf(" MOTOR_OPEN, error\r\n");
        return -1;

    } else if (state == MOTOR_CLOSE) {

        runAbsolute(&motor2, motor_state.m2_end_position);

        motor_state.m_target_state = MOTOR_RUNNING;

        static int running_phase = 0;
        bioc_start_tmr_with_data(bioc_tmr_hdl,
                                 &motor_running_tmr,
                                 motor_running_process_cb,
                                 100 / MS_PER_TICK, &running_phase);
    }

    return 0;
}

static void motor_open_process_cb(int tid, void *arg)
{
    int phase = *(int *)arg;

    if (phase == 0 && getState(&motor2) == IDLE) {
        runAbsolute(&motor1, motor_state.m1_end_position);
        *(int *) arg = 1;
    }

    if (phase == 1 && getState(&motor1) == IDLE) {

        motor_state.m_current_state = MOTOR_OPEN;

        /* set_motor_state_val(); */
        return;
    }

    bioc_start_tmr_with_data(bioc_tmr_hdl,
                             &motor_open_tmr,
                             motor_open_process_cb,
                             100 / MS_PER_TICK, arg);

}

int open_motor(void)
{
    motor_state_t state = get_motor_state();

    if ((getState(&motor1) != IDLE || (getState(&motor2) != IDLE))) {
        printf(" motor running, error\r\n");
        return -1;
    }

    if (state == MOTOR_NONE) {
        printf(" MOTOR_NONE, error\r\n");
        return -1;

    } else if (state == MOTOR_CLOSE) {

        runAbsolute(&motor2, motor_state.m2_begin_position);

        motor_state.m_target_state = MOTOR_OPEN;

        static int open_phase = 0;
        bioc_start_tmr_with_data(bioc_tmr_hdl,
                                 &motor_open_tmr,
                                 motor_open_process_cb,
                                 100 / MS_PER_TICK, &open_phase);
    }

    return 0;
}

static void motor_close_process_cb(int tid, void *arg)
{
    int phase = *(int *)arg;
    if (phase == 0 && getState(&motor2) == IDLE) {
        runAbsolute(&motor1, motor_state.m1_begin_position);
        *(int *) arg = 1;
    }

    if (phase == 1 && getState(&motor1) == IDLE) {

        motor_state.m_current_state = MOTOR_CLOSE;

        /* set_motor_state_val(); */
        return;
    }

    bioc_start_tmr_with_data(bioc_tmr_hdl,
                             &motor_close_tmr,
                             motor_close_process_cb,
                             100 / MS_PER_TICK, arg);

}

int close_motor(void)
{
    motor_state_t state = get_motor_state();

    if ((getState(&motor1) != IDLE || (getState(&motor2) != IDLE))) {
        printf(" motor running, error\r\n");
        return -1;
    }

    if (state == MOTOR_NONE) {
        printf(" MOTOR_NONE, error\r\n");
        return -1;

    } else if (state == MOTOR_OPEN || state == MOTOR_RUNNING) {
        runAbsolute(&motor2, motor_state.m2_begin_position);
        motor_state.m_target_state = MOTOR_CLOSE;

        static int close_phase = 0;
        bioc_start_tmr_with_data(bioc_tmr_hdl,
                                 &motor_close_tmr,
                                 motor_close_process_cb,
                                 100 / MS_PER_TICK, &close_phase);
    }

    return 0;
}

/* m1 水平电机； m2垂直电机 */

/*  m1_begin m1_end m2_begin m2_end  val  state */
/*  0        1      1        0       6    open */
/*  1        0      1        0       10   close */
/*  1        0      0        1       9    running */

static motor_state_t get_motor_state(void)
{
    int m1_begin = !gpio_get_level(SWITCH_LIMITER2_IO_NUM);  //
    int m1_end   = !gpio_get_level(SWITCH_LIMITER1_IO_NUM);

    int m2_begin = !gpio_get_level(SWITCH_LIMITER3_IO_NUM);
    int m2_end   = !gpio_get_level(SWITCH_LIMITER4_IO_NUM);

    int val = ((m1_begin & 0x01) << 3) | ((m1_end & 0x01) << 2) |
              ((m2_begin & 0x01) << 1) | (m2_end & 0x01);

    printf("get motor state: %d, %d %d %d %d\r\n", val, m1_begin, m1_end, m2_begin, m2_end);
    motor_state_t state = MOTOR_NONE;

    if (val == 6) {
        state = MOTOR_OPEN;
    } else if (val == 10) {
        state = MOTOR_CLOSE;
    } else if (val == 9) {
        state = MOTOR_RUNNING;
    }

    return state;
}


/**
 * @brief    -- check_motor_calibration
 * @date     -- 2022-10-17
 * @return   -- 1, ng
 *              0, ok
 */
int check_motor_calibration(void)
{
    return 1;

    motor_state_t state = get_motor_state();

    if ((state == MOTOR_NONE) || (motor_state.m_current_state != motor_state.m_target_state)) {
        memset(&motor_state, 0, sizeof(motor_state));
        printf("state: %d, need_calibrate!!\r\n",  MOTOR_NONE);

        return 1;
    }

    return 0;
}


/* stopper_begin_detected  stopper_end_detected   val */
/* 0                       0                        0 */ // 无上下限
/* 0                       1                        1 */ // 有下限
/* 1                       0                        2 */ // 有上限
/* 1                       1                        3 */ // 有上下限
static void motor_calibration_process_cb(int tid, void *arg)
{
    int phase = *(int *)arg;
    /* 保证电机1 向上运行 , 到最顶端,  然后校准电机2 */

    /* ESP_LOGI(TAG, "cali phase:  %d!!\r\n", phase); */

    if (getState(&motor1) > IDLE || getState(&motor2) > IDLE) {  // motor is active
        goto try;
    }

    /* 1, motor2 如果不在顶部，需要移动到起始位置。 */
    if (phase == 0) {
        if (MOTOR_CALI_VAL(M2_STOPPER_BEGIN_VAL, M2_STOPPER_END_VAL) != MOTOR_IN_BEGIN) {
            runPos(&motor2, 0 - motor_calibrate_step_max); // 向begin位置移动
        } else {
            m2_stopper_begin_detected = 1;
        }

        ESP_LOGI(TAG, "1, motor2 run to begin!!\r\n");
        *(int *)arg = 1;
    }

    /* 2, 对motor1 进行校准 。 */
    if (phase == 1) {  // motor1 calibrate, first move
        if (MOTOR_CALI_VAL(M1_STOPPER_BEGIN_VAL, M1_STOPPER_END_VAL) != MOTOR_IN_BEGIN) {
            runPos(&motor1, 0 - motor_calibrate_step_max); // 向begin位置移动
        } else {
            runPos(&motor1,  motor_calibrate_step_max); // 向end位置移动
        }

        ESP_LOGI(TAG, "2, motor1 run to begin!!\r\n");
        *(int *)arg = 2;
    }

    if (phase == 2) {  // motor2 calibrate
        switch (MOTOR_CALI_VAL(m1_stopper_begin_detected, m1_stopper_end_detected)) {
        case 1:
            runPos(&motor1, 0 - motor_calibrate_step_max); // 向begin位置移动
            break;
        case 2:
            runPos(&motor1, motor_calibrate_step_max); // 向end位置移动
            break;
        case 3:
            ESP_LOGI(TAG, "2, motor1 cali ok!!\r\n");
            runAbsolute(&motor1, motor_state.m1_begin_position);
            *(int *)arg = 3;
            break;
        default:
            break;
        }
    }

    /* 3, 对motor2校准 */
    if (phase == 3) {
        switch (MOTOR_CALI_VAL(m2_stopper_begin_detected, m2_stopper_end_detected)) {
        case 2:
            runPos(&motor2, motor_calibrate_step_max); // 向end位置移动
            break;
        case 3:
            ESP_LOGI(TAG, "3, motor2 cali ok!!\r\n");
            runAbsolute(&motor2, motor_state.m2_begin_position);
            *(int *)arg = 4;
            break;
        default:
            break;
        }
    }

    if (*(int *)arg == 4) {
        motor_state.m_current_state = MOTOR_CLOSE;
        motor_state.m_target_state  = MOTOR_CLOSE;

        /* set_motor_state_val(); */
        printf("motor calibrated ok!!! \r\n");
        return;
    }

try:
        bioc_start_tmr_with_data(bioc_tmr_hdl,
                                 &motor_calibrate_tmr,
                                 motor_calibration_process_cb,
                                 300 / MS_PER_TICK, arg);

}

/*校准第一步： 必须保证motor1 在上面(通过 硬件驱动控制电机往上走)；*/
static void start_motor_calibration(void)
{
    static int calibration_phase = 0;

    bioc_start_tmr_with_data(bioc_tmr_hdl,
                             &motor_calibrate_tmr,
                             motor_calibration_process_cb,
                             100 / MS_PER_TICK, &calibration_phase);

}

static void motor_startup_cb(int tid, void *arg)
{
    enableMotor(&motor1);
    enableMotor(&motor2);

    ESP_LOGI(TAG, "Updating motor_state from NVS");
    if (get_motor_state_val() !=  ESP_OK) {
        ESP_LOGI(TAG, "Updating motor_state FALIED!!");
    }

    if (check_motor_calibration()) {  // need calibrate motor
        start_motor_calibration();
    }
}

void motor_init(void)
{
    init_stepper(&motor1);
    setSpeed(&motor1, 3000, 300);

    init_stepper(&motor2);
    setSpeed(&motor2, 3000, 300);

    bioc_start_tmr(bioc_tmr_hdl,
                   &motor_startup_tmr,
                   motor_startup_cb,
                   5000 / MS_PER_TICK);
}


/// add fun to test
int open_motor2(void)
{
    runAbsolute(&motor2, motor_state.m2_begin_position);
    return 0;
}

int close_motor2(void)
{
    runAbsolute(&motor2, motor_state.m2_end_position);
    return 0;
}

int open_motor1(void)
{
    runAbsolute(&motor1, motor_state.m1_begin_position);
    return 0;
}

int close_motor1(void)
{
    runAbsolute(&motor1, motor_state.m1_end_position);
    return 0;
}
void motor_pause(void)
{
    stop(&motor1);
    stop(&motor2);
}

void motor2_calibrate_toggle(void)
{
    static int step_flag = 0;
    printf("open close motor2 getState: %d, dir: %d, currentPos: %lld\n", getState(&motor2), getDir(&motor2), getPosition(&motor2));
    if (step_flag++ % 2) {
        open_motor2();
    } else {
        close_motor2();
    }
}

void motor1_calibrate_toggle(void)
{
    static int step_flag = 0;
    printf("open close motor1 getState: %d, dir: %d, currentPos: %lld\n", getState(&motor1), getDir(&motor1), getPosition(&motor1));
    if (step_flag++ % 2) {
        open_motor1();

    } else {
        close_motor1();
    }
}

void motor1_test_toggle()
{
    static int step_flag = 0;
    static int step = 200 * 3 * 40;
    printf("motor1 getState: %d, dir: %d, currentPos: %lld\n", getState(&motor1), getDir(&motor1), getPosition(&motor1));
    if (step_flag++ % 2) {
        /* runAbsolute(&motor1, 200 * 3 * 4); // 3圈 */
        step = 0 - step;
        /* runPos(&motor1, step); // 3圈 */
        printf("runPos: %d\n", step);

        disableMotor(&motor1);
        disableMotor(&motor2);

    } else {
        /* stop(&motor1); */
        enableMotor(&motor1);
        enableMotor(&motor2);
    }
}


void motor_cali_test(void)
{
    enableMotor(&motor1);
    enableMotor(&motor2);
    ESP_LOGI(TAG, "Updating motor_state from NVS");
    if (get_motor_state_val() !=  ESP_OK) {
        ESP_LOGI(TAG, "Updating motor_state FALIED!!");
    }

    if (check_motor_calibration()) {  // need calibrate motor
        start_motor_calibration();
    }
}
