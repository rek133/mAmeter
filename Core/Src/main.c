/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "menu_items.h"
#include "watchdog.h"
#include <math.h>
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
  SystemState sysState = {
      /* 小数盘状态 */
      .fractionRemainingSteps = 0,
      .fractionDirection = 0,
      .fractionStepDelay = 10.0f,
      .lastFractionStepTime = 0,

      /* 调零状态 */
      .zeroingStage = 0,
      .zeroingMode = 1,
      .lastDetectTime = 0,
      .initialReverse90Done = 0,
      .is_in_zeroing = 0,
      .reverseSteps = INTEGER_STEPS_PER_DEGREE * 45,
      .reverse90Steps = INTEGER_STEPS_PER_DEGREE * 90,
      .fineTuneSteps = 0,

      /* 小数盘调零状态 */
      .fractionZeroingMode = 0,
      .fractionReverseSteps = INTEGER_STEPS_PER_DEGREE * 20 / 4,
      .fractionReverse90Steps = INTEGER_STEPS_PER_DEGREE * 90 / 4,
      .fractionFineTuneSteps = 0,
      .fractionInitialReverse90Done = 0,
      .isMotorMoving = 0,

      /* 位置值(0.1度单位, 0~3600) */
      .currentDutyCycle = 0.0f,
      .targetDutyCycle = 0.0f,

      /* 串口/菜单 */
      .currentBaudRate = 4800,
      .currentHeadingType = 1,
      .rotValue = 0.0f,
      .uartDataReady = 0,
      .is_in_menu = 0,

      /* 显示 */
      .noDataFlag = 1,
      .decelStepCnt = 0,
      .fractionStepCounter = 0
  };
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void USART3_Send_Byte(uint8_t data);
void USART3_Send_Protocol_Frame_With_CRC(uint8_t frame_data[8]);
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
void Reconfigure_UART_BaudRate(uint32_t baudRate);
/* USER CODE BEGIN PFP */
#define RX_BUFFER_SIZE 1024
uint8_t rxBuffer[RX_BUFFER_SIZE];
uint16_t rxIndex = 0;

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 必须在main()中初始化DWT计数器（开机只执行一次）
void DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

/* USER CODE END 1 */


  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
    DWT_Init();
  MX_GPIO_Init();
  MX_TIM1_Init();
    MX_DMA_Init();
  MX_USART1_UART_Init();
    MX_USART3_UART_Init(); // 新增：初始化USART3半双工模式
    HAL_UART_Receive_DMA(&huart1, rxBuffer, RX_BUFFER_SIZE);
  __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

  /* USER CODE BEGIN 2 */
    // 初始化背光控制
  Backlight_Init();
    TM1637_SetBrightness(4);
  TM1637_display(21,21,21,21); // 开机清空显示，避免残留
    // 上电后立即读取Flash中的偏差值
  Flash_Load_ZeroOffset();         // 加载整数偏移
    Flash_Load_FractionZeroOffset(); // 加载小数偏移
    Flash_Load_BaudRate();           // 加载波特率
    Reconfigure_UART_BaudRate(sysState.currentBaudRate);
    Flash_Load_HeadingType();        // 加载语句类型

    uint8_t send_data[8] = {0x05, 0x00, 0x80, 0x00, 0x00, 0x01, 0xC1, 0x00};     //启用uart
    uint8_t send_data1[8] = {0x05, 0x00, 0xEC, 0x16, 0x01, 0x00, 0x53, 0x00};    //取消插值微分，提高转速上限

     USART3_Send_Protocol_Frame_With_CRC(send_data);
    USART3_Send_Protocol_Frame_With_CRC(send_data1);

    /* 看门狗初始化并使能: 3s 超时 */
    Watchdog_Init();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  { 
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 1 */

/* 单字节CRC(多项式0x07, LSB先行), 结果写入datagram最后一字节 */
void swuart_calcCRC(uint8_t *datagram, uint8_t datagramLength)
{
    uint8_t *crc = datagram + (datagramLength - 1);
    uint8_t currentByte;
    int i, j;

    *crc = 0;
    for (i = 0; i < (datagramLength - 1); i++) {
        currentByte = datagram[i];
        for (j = 0; j < 8; j++) {
            if (((*crc >> 7) & 0x01) ^ (currentByte & 0x01)) {
                *crc = (*crc << 1) ^ 0x07;
            } else {
                *crc = (*crc << 1);
            }
            currentByte >>= 1;
        }
    }
}

/* USART3半双工发送单字节(先切到发送模式) */
void USART3_Send_Byte(uint8_t data)
{
    HAL_HalfDuplex_EnableTransmitter(&huart3);
    HAL_UART_Transmit(&huart3, &data, 1, 100);
}

/* 发送8字节TMC协议帧: 先算CRC填入末字节, 再逐字节发送(间隔1ms适配TMC2225) */
void USART3_Send_Protocol_Frame_With_CRC(uint8_t frame_data[8])
{
    uint8_t protocol_frame[8];
    uint8_t i;

    for (i = 0; i < 8; i++) {
        protocol_frame[i] = frame_data[i];
    }
    swuart_calcCRC(protocol_frame, 8);
    for (i = 0; i < 8; i++) {
        USART3_Send_Byte(protocol_frame[i]);
        HAL_Delay(1);
    }
}
/* USER CODE END 1 */



/* USER CODE BEGIN 4 */

/* 支持的航向语句类型 */
static const char *const heading_types[] = {"HDG", "HDT", "HDM", "THS", "PHDT", "CHDT", "EHDT"};

/* 解析NMEA语句中的航向字段(取第一个逗号后、第二个逗号前的值, 保留1位小数) */
float parseHeading(const char *data)
{
    const char *start = strchr(data, ',');
    const char *end;
    char valueStr[16];
    uint8_t len;

    if (start == NULL) {
        return 0.0f;
    }
    start++;
    end = strchr(start, ',');
    if (end == NULL) {
        return 0.0f;
    }
    len = end - start;
    if (len > sizeof(valueStr) - 1) {
        len = sizeof(valueStr) - 1;
    }
    strncpy(valueStr, start, len);
    valueStr[len] = '\0';
    return roundf(atof(valueStr) * 10.0f) / 10.0f;
}

/* NMEA校验和验证: $与*之间逐字节异或 == *后十六进制值 */
uint8_t validateChecksum(const char *data)
{
    uint8_t calculated = 0;
    const char *p = data + 1;
    uint8_t received;

    if (data[0] != '$' || strchr(data, '*') == NULL) {
        return 0;
    }
    while (*p && *p != '*' && (p - data) < 128) {
        calculated ^= *p;
        p++;
    }
    if (*p != '*') {
        return 0;
    }
    received = (uint8_t)strtol(p + 1, NULL, 16);
    return (calculated == received);
}

/* 按换行切分DMA收到的NMEA串, 校验和通过且语句类型匹配时更新航向值 */
void processReceivedData(uint16_t len)
{
    char *start = (char *)rxBuffer;
    char *end;
    const char *currentType;

    rxBuffer[len] = '\0';
    currentType = heading_types[sysState.currentHeadingType];

    while ((end = strchr(start, '\n')) != NULL) {
        *end = '\0';
        if (validateChecksum(start) && strstr(start, currentType) != NULL) {
            sysState.rotValue = parseHeading(start);
            sysState.uartDataReady = 1;
        } else {
            sysState.uartDataReady = 0;
        }
        start = end + 1;
    }

    /* 剩余不完整帧移到缓冲区头, 等待后续数据拼帧 */
    if (start < (char *)(rxBuffer + len)) {
        uint16_t remaining = (char *)(rxBuffer + len) - start;
        memmove(rxBuffer, start, remaining);
        rxIndex = remaining;
    } else {
        rxIndex = 0;
    }
}

/* USART1 DMA空闲中断: 一帧收完, 记录长度并重启DMA */
void USART1_IRQHandler(void)
{
    uint16_t len;

    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        len = RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);
        if (len > 0 && len < RX_BUFFER_SIZE) {
            sysState.uartDataLength = len;
            HAL_UART_DMAStop(&huart1);
            HAL_UART_Receive_DMA(&huart1, rxBuffer, RX_BUFFER_SIZE);
        }
    }
}

/* 重新配置USART1波特率(改配置需重启DMA接收) */
void Reconfigure_UART_BaudRate(uint32_t baudRate)
{
    HAL_UART_DMAStop(&huart1);
    huart1.Init.BaudRate = baudRate;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
    HAL_UART_Receive_DMA(&huart1, rxBuffer, RX_BUFFER_SIZE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

/* USER CODE END 4 */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
