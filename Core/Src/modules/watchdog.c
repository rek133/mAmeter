#include "watchdog.h"
#include "main.h"

static IWDG_HandleTypeDef hiwdg;

/*
 * 看门狗初始化: 超时 3s
 *   LSI 约 40kHz, 预分频 64, 重载 1875
 * 注意: IWDG 一旦启动无法用软件关闭, 调试单步时须在 Keil
 *       Options->Debug 勾选 Freeze watchdog, 否则一停就复位
 */
void Watchdog_Init(void)
{
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 1875;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

void Watchdog_Feed(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}
