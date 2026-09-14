#ifndef PWM_INPUT_H
#define PWM_INPUT_H
#include "shared_data.h"
#include "cmsis_os.h"
#include <math.h>
#include <stdio.h>
#include "tim.h"
#include "main.h"

// PWM输入处理参数
#define PWM_FILTER_ALPHA 0.8f       // 滤波器系数(0-1，越小滤波越强)
#define PWM_UPDATE_THRESHOLD 5      // 占空比变化阈值(%，小于此值不更新)
#define MIN_PWM_UPDATE_INTERVAL 50  // 最小PWM更新间隔(ms)

void PWM_Input_CaptureCallback(TIM_HandleTypeDef *htim);
void PWM_Input_Init(void);
#endif /* PWM_INPUT_H */