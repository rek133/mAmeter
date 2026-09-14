#ifndef ZERO_CALIBRATION_H
#define ZERO_CALIBRATION_H
#include "shared_data.h"
#include "stdint.h"
#include "stepper_motor.h"
#include "cmsis_os.h"
#include "main.h"
#include "key_manager.h"
#include "menu_items.h"

void zeroingStateMachine(void);
void enterZeroingMode(void);
void enterAutoZeroingMode(void);

// ********** 根据你的STM32型号修改地址 **********
// 参数存储页: 0x0800F800 (1KB, 4参数同页, 页首=波特率, 擦除按页对齐)
#define BAUD_RATE_FLASH_ADDR       0x0800F800  // 波特率(页首)
#define HEADING_TYPE_FLASH_ADDR    0x0800F804  // 航向类型
#define FRACTION_OFFSET_FLASH_ADDR 0x0800F808  // 小数偏差
#define ZERO_OFFSET_FLASH_ADDR     0x0800F80C  // 整数偏差

#define DEFAULT_ZERO_OFFSET    0           // 默认偏差值（首次上电用）
#define DEFAULT_FRACTION_ZERO_OFFSET 0
#define DEFAULT_HEADING_TYPE        0           // CF子菜单默认值（HDG）
#define DEFAULT_BAUD_RATE           4800        // 波特率默认值

// 函数声明
void Flash_Save_ZeroOffset(void);  // 保存偏差值到Flash
void Flash_Load_ZeroOffset(void);  // 从Flash加载偏差值
void Flash_Save_FractionZeroOffset(void);
void Flash_Load_FractionZeroOffset(void);
void Flash_Save_HeadingType(uint8_t headingType);  // 新增：保存航向类型
void Flash_Load_HeadingType(void);              // 新增：加载航向类型
void Flash_Save_BaudRate(uint32_t baudRate);       // 新增：保存波特率
void Flash_Load_BaudRate(void);                // 新增：加载波特率
#endif /* ZERO_CALIBRATION_H */