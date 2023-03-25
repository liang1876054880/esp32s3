#ifndef MOTOR_H
#define MOTOR_H
#include <stdio.h>
#include <stdint.h>

#define SWITCH_LIMITER1_IO_NUM  5
#define SWITCH_LIMITER2_IO_NUM  16

#define SWITCH_LIMITER3_IO_NUM  36
#define SWITCH_LIMITER4_IO_NUM  14


typedef enum {
    MOTOR_OPEN,
    MOTOR_CLOSE,
    MOTOR_RUNNING,
    MOTOR_NONE,
} motor_state_t;

typedef struct {
    int m1_begin_position;
    int m1_end_position;
    int m1_calibrated;

    // int m1_direction;        // 电机转向
    int m1_current_position; // 当前位置
    int m1_target_position;  // 目标位置

    int m2_begin_position;
    int m2_end_position;
    int m2_calibrated;

    // int m2_direction;        // 电机转向
    int m2_current_position; // 当前位置
    int m2_target_position;  // 目标位置

    motor_state_t m_current_state;  // 当前状态
    motor_state_t m_target_state;   // 目标状态

} motor_calibrate_t;

void motor_stopper_event_process(int stopper_id, int event, int data);

#endif

