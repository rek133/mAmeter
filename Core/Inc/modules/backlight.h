#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "main.h"
#include "stm32f1xx_hal.h"
#include "tim.h"

// 背光亮度等级定义 (0-4级)
#define BACKLIGHT_LEVELS 5

// 背光控制引脚定义
#define BACKLIGHT_PWM_TIM_HANDLE  htim1
#define BACKLIGHT_PWM_CHANNEL     TIM_CHANNEL_1

// 背光状态结构体
typedef struct {
    uint8_t current_level;      // 当前亮度等级 (0-7)
} Backlight_State_t;

// 函数声明
void Backlight_Init(void);
void Backlight_SetLevel(uint8_t level);
void Backlight_Increase(void);
void Backlight_Decrease(void);


// 引脚定义 - 用户需要根据实际硬件修改
#define TM1637_CLK_PIN  GPIO_PIN_6
#define TM1637_CLK_PORT GPIOB
#define TM1637_DIO_PIN  GPIO_PIN_7
#define TM1637_DIO_PORT GPIOB

// 引脚操作宏定义
#define TM1637_CLK_HIGH() HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_SET)
#define TM1637_CLK_LOW()  HAL_GPIO_WritePin(TM1637_CLK_PORT, TM1637_CLK_PIN, GPIO_PIN_RESET)
#define TM1637_DIO_HIGH() HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_SET)
#define TM1637_DIO_LOW()  HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, GPIO_PIN_RESET)
#define TM1637_DIO_WRITE(x) HAL_GPIO_WritePin(TM1637_DIO_PORT, TM1637_DIO_PIN, (x))
#define TM1637_DIO_READ() HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN)

// 函数声明
void TM1637_display(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void TM1637_SetBrightness(uint8_t level);
void convertToDisplayCode(float value, uint8_t *dispCode);

#endif
