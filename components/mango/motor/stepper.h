#pragma once

#ifndef STEPPER_H
#define STEPPER_H

#include "stdint.h"
#include "driver/timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "string.h"
#include "math.h"

//#define STEP_DEBUG

#define NS_TO_T_TICKS(x) (x/25)
#define TIMER_F          20000000ULL
#define TICK_PER_S       40000000ULL

#define TIMER_F          500000ULL
#define TICK_PER_S       100000ULL

enum motor_status {
    DISABLED,
    IDLE,
    ACC,
    COAST,
    DEC,
    INF,
};

enum dir {
    CW,
    CCW
};

typedef enum {
    MICROSTEP_1   = 0x1,
    MICROSTEP_2   = 0x2,
    MICROSTEP_4   = 0x4,
    MICROSTEP_8   = 0x8,
    MICROSTEP_16  = 0x10,
    MICROSTEP_32  = 0x20,
    MICROSTEP_64  = 0x40,
    MICROSTEP_128 = 0x80,
    MICROSTEP_256 = 0x100,
} micro_stepping_t;

/* HW configuration struct */
typedef struct {
    uint8_t         step_p;      // step signal gpio
    uint8_t         dir_p;       // dir signal gpio
    uint8_t         en_p;        // enable signal gpio
    timer_group_t   timer_group; // timer group, useful if we are controlling more than 2 steppers
    timer_idx_t     timer_idx;   // timer index, useful if we are controlling 2steppers
    micro_stepping_t miStep;
} stepper_config_t;

typedef struct {
    uint32_t    stepInterval;    // step interval in ns/25   40M
    int32_t     accelC;
    uint32_t    rest;            // step interval increase during acc/dec phase
    uint32_t    stepCnt;         // step counter
    uint32_t    accEnd;          // when to end acc and start coast
    uint32_t    accLim;
    uint32_t    coastEnd;        // when to end coast and start decel
    uint32_t    stepsToGo;       // steps we need to take
    float       speed;           // speed in steps*second^-1
    float       acc;             // acceleration in steps*second^-2
    uint64_t    enOffTime;
    volatile uint8_t     status;
    bool        dir;
    bool        homed;
    bool        runInfinite;
} ctrl_var_t;

typedef struct {
    stepper_config_t conf;
    ctrl_var_t ctrl;

    uint64_t currentPos;//absolute position
    bool timerStarted;
    bool state;

    esp_timer_handle_t dyingTimer;
    TaskHandle_t enTask;
} dev_stepper_t;

/** @brief PRIVATE: Step interval calculation
 *  @param speed maximum movement speed
 *  @param accTimeMs acceleration time in ms
 *  @param target target position
 */
void calc(dev_stepper_t *dev, uint32_t target);

/** @brief sets En GPIO
 *  @param state 0-LOW,1-HIGH
 *  @return void
 */
void setEn(dev_stepper_t *dev, bool state);

/** @brief sets Dir GPIO
 *  @param state 0-CW 1-CCW
 */
void setDir(dev_stepper_t *dev, bool state);

bool xISR(dev_stepper_t *dev);

void enTimer(dev_stepper_t *dev);


/** @brief initialize GPIO and Timer peripherals
 *  @param stepP step pulse pin
 *  @param dirP direction signal pin
 *  @param enP enable signal Pin
 *  @param group timer group to use (0 or 1)
 *  @param index which timer to use (0 or 1)
 *  @param microstepping microstepping performed by the driver, used for more accuracy
 *  @param stepsPerRot how many steps it takes for the motor to move 2Pi rads. this can be also used instead of microstepping parameter
 */
// void init(uint8_t, uint8_t, uint8_t, timer_group_t, timer_idx_t, microStepping_t microstep, uint16_t stepsPerRot);
void init_stepper(dev_stepper_t *dev);

/** @brief runs motor to relative position in steps
 *  @param relative number of steps to run, negative is reverse
 */
esp_err_t runPos(dev_stepper_t *dev, int32_t relative);

/** @brief sets motor speed
 *  @param speed speed in steps per second
 *  @param accTimeMs acceleration time in ms
 */
void setSpeed(dev_stepper_t *dev, uint32_t speed, uint16_t accT);

/** @brief sets motor speed and accel in radians
 *  @param speed speed rad*s^-1
 *  @param accTimeMs acceleration in rad*s^-2
 */
void setSpeedRad(dev_stepper_t *dev, float speed, float acc);

/** @brief set EN pin 1, stops movement
*/
void disableMotor(dev_stepper_t *dev);

/** @brief set EN pin to 0, enables movement
*/
void enableMotor(dev_stepper_t *dev);

/** @brief returns current state
 *  @return motor_status enum
 */
uint8_t getState(dev_stepper_t *dev);

uint8_t getDir(dev_stepper_t *dev);

/** @brief run motor to position in absolute coordinates (steps)
 *  @param postition absolute position in steps from homing position (must be positive);
 *  @return true if motor can run immediately, false if it is currently moving
 */
bool runAbsolute(dev_stepper_t *dev, uint32_t position);

/** @brief returns current absolute position
 *  @return current absolute postion in steps
 */
uint64_t getPosition(dev_stepper_t *dev);

/** @brief resets absolute pos to 0
*/
void resetAbsolute(dev_stepper_t *dev);

/** @brief returns current speed
*/
uint16_t getSpeed(dev_stepper_t *dev);

/** @brief returns current acceleration time in ms
*/
uint16_t getAcc(dev_stepper_t *dev);

/** @brief stops the motor dead, but stays enabled
*/
void stop(dev_stepper_t *dev);

/** @brief sets the timeout after which motor is disabled
*/
void setEnTimeout(dev_stepper_t *dev, uint64_t timeout);

#endif
