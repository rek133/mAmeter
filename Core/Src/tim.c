/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
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
/* Includes ------------------------------------------------------------------*/
#include "tim.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

TIM_HandleTypeDef htim1;  // 统一使用htim1句柄

/* TIM1 init function (PWM模式) */
void MX_TIM1_Init(void)
{
  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};  // PWM输出用OC配置结构体

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  // TIM1基本配置
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 71;          // 分频系数：72MHz/(71+1)=1MHz
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;            // 自动重载值：1MHz/(999+1)=1kHz PWM频率
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;   // TIM1是高级定时器，需配置重复计数器（0表示无重复）
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  
  // 初始化PWM模式
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  // 主模式配置
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  // PWM通道1配置
  sConfigOC.OCMode = TIM_OCMODE_PWM1;          // PWM模式1：CNT < CCR时输出有效电平
  sConfigOC.Pulse = 500;                       // 占空比：500/1000=50%（可根据需求调整）
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;   // 有效电平为高
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM1 PWM MSP初始化
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* tim_pwmHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(tim_pwmHandle->Instance==TIM1)
  {
    /* USER CODE BEGIN TIM1_MspInit 0 */

    /* USER CODE END TIM1_MspInit 0 */
    // 启用TIM1时钟
    __HAL_RCC_TIM1_CLK_ENABLE();
    
    // 启用GPIOA时钟（PA8对应TIM1_CH1）
    __HAL_RCC_GPIOA_CLK_ENABLE();
    
    /**TIM1 GPIO配置
    PA8     ------> TIM1_CH1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;      // 复用推挽输出（PWM必须）
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    

    HAL_NVIC_SetPriority(TIM1_UP_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);

    /* USER CODE BEGIN TIM1_MspInit 1 */

    /* USER CODE END TIM1_MspInit 1 */
  }
}

/**
  * @brief TIM1 MspDeInit
  */
void HAL_TIM_PWM_MspDeInit(TIM_HandleTypeDef* tim_pwmHandle)
{
  if(tim_pwmHandle->Instance==TIM1)
  {
    /* USER CODE BEGIN TIM1_MspDeInit 0 */

    /* USER CODE END TIM1_MspDeInit 0 */
    // 关闭TIM1时钟
    __HAL_RCC_TIM1_CLK_DISABLE();
    
    // 复位GPIOA引脚
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8);
    
    /* 若启用了中断，关闭NVIC
    HAL_NVIC_DisableIRQ(TIM1_UP_IRQn);
    */
    /* USER CODE BEGIN TIM1_MspDeInit 1 */

    /* USER CODE END TIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/* USER CODE END 1 */
