#include "stepper.h"
#include "esp_log.h"

/* #define STEP_DEBUG */

#ifdef STEP_DEBUG
#define STEP_LOGI(...) ESP_LOGI(__VA_ARGS__)
#define STEP_LOGW(...) ESP_LOGW(__VA_ARGS__)
#define STEP_LOGE(...) ESP_LOGE(__VA_ARGS__)
#else
#define STEP_LOGI(...) while (0)
#define STEP_LOGW(...) while (0)
#define STEP_LOGE(...) ESP_LOGE(__VA_ARGS__)
#endif

/* dev_stepper_t step =  { */
/*     .ctrl = { */
/*         .stepInterval = 40000, */
/*         .rest         = 0, */
/*         .stepCnt      = 0, */
/*         .stepsToGo    = 0, */
/*         .speed        = 100, */
/*         .acc          = 100, */
/*         .enOffTime    = 10000L, */
/*         .status       = DISABLED, */
/*         .dir          = CW, */
/*         .homed        = false, */
/*         .runInfinite  = false, */
/*  */
/*     }, */
/*  */
/*     .conf = { */
/*         .step_p      = 14, */
/*         .dir_p       = 27, */
/*         .en_p        = 26, */
/*         .timer_group = TIMER_GROUP_0, */
/*         .timer_idx   = TIMER_0, */
/*         .miStep      = MICROSTEP_1, */
/*     }, */
/*  */
/* }; */
/*  */
void setEn(dev_stepper_t *dev, bool state)
{
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)dev->conf.en_p, state));
}

void setDir(dev_stepper_t *dev, bool state)
{
    dev->ctrl.dir = state;
    ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)dev->conf.dir_p, state));
}

void disableMotor(dev_stepper_t *dev)
{
    setEn(dev, true);
    STEP_LOGI("stepper", "Disabled");
    dev->ctrl.status = DISABLED;
}

/** @brief static wrapper for ISR function
*/
static bool xISRwrap(void *arg)
{
    return xISR((dev_stepper_t *)arg);
}

/** @brief static wrapper for En timer
*/
static void enTimerWrap(void *arg)
{
    enTimer((dev_stepper_t *)arg);
}

/** @brief enableMotor wrapper
*/
static void _disableMotor(void *arg)
{
    disableMotor((dev_stepper_t *)arg);
}

void enableMotor(dev_stepper_t *dev)
{
    setEn(dev, false);
    dev->ctrl.status = IDLE;
    STEP_LOGI("stepper", "Enabled");
    dev->timerStarted = 0;
}

/* Timer callback, used for generating pulses and calculating speed profile in real time */
bool xISR(dev_stepper_t *dev)
{
    gpio_set_level((gpio_num_t)dev->conf.step_p, (dev->state = !dev->state)); //step pulse
    //add and substract one step
    if (dev->state == 0) {
        return 0; //just turn off the pin in this iteration
    }

    dev->ctrl.stepCnt++;
    dev->ctrl.stepsToGo--;

    //we are done
    if (dev->ctrl.stepsToGo == 0) {
        timer_pause(dev->conf.timer_group, dev->conf.timer_idx);  //stop the timer
        dev->ctrl.status = IDLE;
        dev->ctrl.stepCnt = 0;
        gpio_set_level((gpio_num_t)dev->conf.step_p, 0); //this should be enough for driver to register pulse

        return 0;
    }

    //absolute coord handling
    if (dev->ctrl.dir == CW) {
        dev->currentPos++;
    } else if (dev->currentPos > 0) {
        dev->currentPos--; //we cant go below 0, or var will overflow
    }

    if (dev->ctrl.accelC > 0 && dev->ctrl.accelC < dev->ctrl.accEnd) { //we are accelerating
        uint32_t oldInt = dev->ctrl.stepInterval;
        dev->ctrl.stepInterval = oldInt - (2 * oldInt + dev->ctrl.rest) / (4 * dev->ctrl.accelC + 1);
        dev->ctrl.rest = (2 * oldInt + dev->ctrl.rest) % (4 * dev->ctrl.accelC + 1);
        dev->ctrl.accelC++;
        dev->ctrl.status = ACC;  //we are accelerating, note that

    } else if (dev->ctrl.stepCnt > dev->ctrl.coastEnd) { //we must be deccelerating then

        uint32_t oldInt = dev->ctrl.stepInterval;
        dev->ctrl.stepInterval = (int32_t)oldInt - ((2 * (int32_t)oldInt + (int32_t)dev->ctrl.rest) / (4 * dev->ctrl.accelC + 1));
        dev->ctrl.rest = (2 * (int32_t)oldInt + (int32_t)dev->ctrl.rest) % (4 * dev->ctrl.accelC + 1);
        dev->ctrl.accelC++;
        dev->ctrl.status = DEC; //we are deccelerating

    } else { //we are coasting
        dev->ctrl.status = COAST; //we are coasting
        dev->ctrl.accelC = (int32_t)dev->ctrl.coastEnd * -1;
    }

    //set alarm to calculated interval
    timer_set_alarm_value(dev->conf.timer_group, dev->conf.timer_idx, dev->ctrl.stepInterval / 2);
    return 1;
}

void enTimer(dev_stepper_t *dev)
{
    while (1) {
        if (!dev->timerStarted && dev->ctrl.status == IDLE) {
            //create the en timer
            esp_timer_create_args_t t_arg = {
                .callback = _disableMotor,
                .arg = dev,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "En timer",
                .skip_unhandled_events = 1,
            };
            STEP_LOGI("stepper", "Enable timer started");
            esp_timer_create(&t_arg, &dev->dyingTimer);
            esp_timer_start_once(dev->dyingTimer, dev->ctrl.enOffTime);
            dev->timerStarted = 1;
        } else if (dev->ctrl.status > IDLE) {
            esp_timer_delete(dev->dyingTimer);
        }
        vTaskDelay(100);
    }
}

void init_stepper(dev_stepper_t *dev)
{
    dev->ctrl.status = 0;

    uint64_t mask = (1ULL << dev->conf.step_p) | (1ULL << dev->conf.dir_p) |
                    (1ULL << dev->conf.en_p);   //put gpio pins in bitmask
    gpio_config_t gpio_conf = { //dev->config gpios
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    //set the gpios as per gpio_conf
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    timer_config_t timer_conf = {
        .alarm_en    = TIMER_ALARM_EN,      // we need alarm
        .counter_en  = TIMER_PAUSE,         // dont start now lol
        .intr_type   = TIMER_INTR_LEVEL,    // interrupt
        .counter_dir = TIMER_COUNT_UP,      // count up duh
        .auto_reload = TIMER_AUTORELOAD_EN, // reload pls
        .divider     = 2,                   // 25ns resolution
    };

    ESP_ERROR_CHECK(timer_init(dev->conf.timer_group, dev->conf.timer_idx, &timer_conf)); // init the timer
    ESP_ERROR_CHECK(timer_set_counter_value(dev->conf.timer_group, dev->conf.timer_idx, 0));   // set it to 0
    ESP_ERROR_CHECK(timer_isr_callback_add(dev->conf.timer_group, dev->conf.timer_idx, xISRwrap, dev, 0)); // add callback fn to run when alarm is triggrd
}

esp_err_t runPos(dev_stepper_t *dev, int32_t relative)
{
    if (!relative) { //why would u call it with 0 wtf
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (dev->ctrl.status > IDLE) {
        //we are running, we need to adjust steps accordingly, for now just stop the movement
        STEP_LOGW("stepper", "Finising previous move, this command will be ignored");
        return ESP_ERR_NOT_SUPPORTED;
    }

    dev->ctrl.homed = false;          //we are not longer homed

    if (dev->ctrl.status == DISABLED) { //if motor is disabled, enable it
        enableMotor(dev);
    }

    dev->ctrl.status = ACC;
    setDir(dev, relative < 0); //set CCW if <0, else set CW
    calc(dev, abs(relative));  //calculate velocity profile

    ESP_ERROR_CHECK(timer_set_alarm_value(dev->conf.timer_group, dev->conf.timer_idx, dev->ctrl.stepInterval));  //set HW timer alarm to stepinterval
    ESP_ERROR_CHECK(timer_start(dev->conf.timer_group, dev->conf.timer_idx));   //start the timer

    /* dev->currentPos += relative; */
    return ESP_OK;
}

void setSpeed(dev_stepper_t *dev, uint32_t speed, uint16_t accT)
{
    dev->ctrl.speed = speed / (200.0 * (float)dev->conf.miStep);
    dev->ctrl.acc = dev->ctrl.speed / (accT / 1000.0);
    STEP_LOGI("stepper", "Speed set: %f %f", dev->ctrl.speed, dev->ctrl.acc);
}

void setSpeedRad(dev_stepper_t *dev, float speed, float acc)
{
    dev->ctrl.speed = speed;
    dev->ctrl.acc = acc;
    STEP_LOGI("stepper", "Speed set: %f %f", dev->ctrl.speed, dev->ctrl.acc);
}

void calc(dev_stepper_t *dev, uint32_t target)
{
    //calculate number of steps needed for acceleration
    dev->ctrl.accEnd = (dev->ctrl.speed * dev->ctrl.speed) / (2.0 * 0.0005 * dev->ctrl.acc);

    //calculate the limit value of steps needed for acceleration
    //(used if we dont have enough steps to perform full acc to max speed)
    dev->ctrl.accLim = (target * dev->ctrl.acc) / (dev->ctrl.acc * 2);

    if (dev->ctrl.accEnd < dev->ctrl.accLim) { //acceleration is limited by max speed
        dev->ctrl.coastEnd = target - dev->ctrl.accEnd; //calculate when to start deccelerating in steps
    } else {                        //acceleration is limited by start of the deceleration (triangular profile)
        dev->ctrl.coastEnd = dev->ctrl.accEnd = target - dev->ctrl.accLim; //no coast phase, we are limited by number of steps
    }

    //init vars
    dev->ctrl.accelC = 1;
    dev->ctrl.rest = 0;

    //calculate initial interval, also known as c0
    dev->ctrl.stepInterval = (float)TIMER_F * sqrt((((4 * 3.14) / (200.0 * (float)dev->conf.miStep)) / dev->ctrl.acc));

    //set steps we will take
    dev->ctrl.stepsToGo = target;

    //debug
    STEP_LOGI("calc", "acc end:%u coastend:%u acclim:%u stepstogo:%u speed:%f acc:%f int: %u",
              dev->ctrl.accEnd, dev->ctrl.coastEnd, dev->ctrl.accLim,
              dev->ctrl.stepsToGo, dev->ctrl.speed, dev->ctrl.acc, dev->ctrl.stepInterval);

    STEP_LOGI("calc", "int: %u rest %u", dev->ctrl.stepInterval, dev->ctrl.rest);

    //init old interval
    //uint32_t oldInt=dev->ctrl.stepInterval;
}

uint8_t getState(dev_stepper_t *dev)
{
    return dev->ctrl.status;
}

uint8_t getDir(dev_stepper_t *dev)
{
    return dev->ctrl.dir;
}

bool runAbsolute(dev_stepper_t *dev, uint32_t position)
{
    if (getState(dev) > IDLE) {  //we are already moving, so stop it
        stop(dev);
    }

    while (getState(dev) > IDLE) {
        //waiting for idle, watchdog should take care of inf loop if it occurs
    }                              //shouldnt take long tho

    if (position == dev->currentPos) {
        return ESP_FAIL;
    }

    runPos(dev, position - dev->currentPos); //run to new position

    return ESP_OK;
}

uint64_t getPosition(dev_stepper_t *dev)
{
    return dev->currentPos;
}

void resetAbsolute(dev_stepper_t *dev)
{
    dev->currentPos = 0;
}

uint16_t getSpeed(dev_stepper_t *dev)
{
    return dev->ctrl.speed;
}

uint16_t getAcc(dev_stepper_t *dev)
{
    return dev->ctrl.acc;
}

void stop(dev_stepper_t *dev)
{
    dev->ctrl.stepsToGo = 1; //no more steps needed, xISR should take care of the rest
    dev->ctrl.status = IDLE;
    //todo: deccelerate
}

void setEnTimeout(dev_stepper_t *dev, uint64_t timeout)
{
    if (timeout == 0) {
        vTaskDelete(dev->enTask);
    }

    dev->ctrl.enOffTime = timeout * 1000;

    xTaskCreate(enTimerWrap, "En timer task", 2048, dev, 7, &dev->enTask);
}

