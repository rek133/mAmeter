#ifndef WATCHDOG_H
#define WATCHDOG_H

#include "stm32f1xx_hal.h"

/*
 * 独立看门狗 (IWDG)
 *   STM32F103 LSI 约 40kHz
 *   预分频 64  -> 计数时钟 625Hz
 *   重载   1875 -> 超时 (1875+1)/625 = 3.0s
 */

void Watchdog_Init(void);   /* 初始化并使能 IWDG (启动后无法软件关闭) */
void Watchdog_Feed(void);   /* 喂狗: 刷新计数器 */

#endif /* WATCHDOG_H */
