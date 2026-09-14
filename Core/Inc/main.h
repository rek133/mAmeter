/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "shared_data.h"
#include "cmsis_os.h"
#include "tim.h"
#include "gpio.h"
#include "backlight.h"	
#include "pwm_input.h"
#include "zero_calibration.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#define RX_BUFFER_SIZE 256  // 宏定义也需要暴露，供外部使用缓冲区大小时参考

//extern uint8_t rxBuffer[RX_BUFFER_SIZE];  // 声明外部可访问的缓冲区
//extern uint16_t rxIndex;                  // 声明外部可访问的索引
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
