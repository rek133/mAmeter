#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H
#include "stdint.h"
#include "shared_data.h"
#include "cmsis_os.h"
#include "math.h"
#include "main.h"

void delay_us(uint32_t us);
void stepIntegerMotor(void);
void setIntegerMotorDirection(uint8_t clockwise);
void stepFractionMotor(void);
void setFractionMotorDirection(uint8_t clockwise);
void moveMotorTask(void);

#endif /* STEPPER_MOTOR_H */