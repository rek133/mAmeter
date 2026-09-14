#include "pwm_input.h"


void PWM_Input_Init(void){
      // 启动定时器输入捕获
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim1, TIM_CHANNEL_1, TIM_INPUTCHANNELPOLARITY_RISING);
    // 使能捕获/比较中断
    __HAL_TIM_ENABLE_IT(&htim1, TIM_IT_CC1);
}
