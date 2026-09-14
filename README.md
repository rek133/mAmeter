# mAmeter · STM32 双转盘指示器

基于 **STM32F103** 的「双转盘」指示器固件：接收航向数据后，用**整数盘 + 小数盘**两路步进电机分别指示航向的整数位与小数位，并配 TM1637 数码管显示。

> 使用 STM32CubeMX + FreeRTOS（CMSIS-OS2），Keil MDK-ARM 工程。

## ✨ 功能特性

- **双转盘**：整数盘 + 小数盘，两路步进电机（TMC2225，16 细分）
- **自动调零**：上电读零位传感器回零
- **航向输入**：UART 接收 NMEA 0183 航向数据
- **数码显示**：TM1637 四位数码管
- **按键菜单**：正常 / 菜单 / 调零 三种模式，可设置波特率、航向类型、整数偏差、小数偏差
- **参数掉电保存**：写入片内 Flash（`0x0800F800`）
- **背光调节**：5 档
- **看门狗**：IWDG（约 3s）

## 🔧 硬件

| 项目 | 说明 |
| --- | --- |
| MCU | STM32F103 |
| 步进驱动 | TMC2225 ×2（整数盘 PA5 DIR / PA6 STEP；小数盘 PB0 DIR / PB1 STEP） |
| 零位传感器 | PB4（低有效） |
| 驱动使能 | PB5 |
| 显示 | TM1637（PB6 CLK / PB7 DIO） |
| 背光 | TIM1 PWM 5 档（0 / 250 / 500 / 750 / 999） |

引脚定义见根目录 `mAmeter.ioc`。

## 🧱 软件结构

- **框架**：STM32CubeMX 生成骨架 + FreeRTOS（CMSIS-OS2）
- **IDE**：Keil MDK-ARM —— 工程文件 `MDK-ARM/mAmeter.uvprojx`
- **源码编码**：GBK
- **任务划分**：
  - `defaultTask`（1ms）：调零状态机
  - `sensorTask`（20ms）：NMEA 报文解析
  - `stepperTask`（1ms）：步进电机脉冲驱动
  - `buttonTask`（50ms）：按键管理 + TM1637 显示 + 喂狗

## 📁 目录

```
Core/          用户代码（main.c / freertos.c / modules/…）
Drivers/       STM32 HAL 驱动 + CMSIS
Middlewares/   FreeRTOS
MDK-ARM/       Keil MDK 工程
mAmeter.ioc    CubeMX 工程文件
TMC2225.pdf    TMC2225 驱动芯片数据手册
```

## 🛠 编译

用 **Keil MDK-ARM** 打开 `MDK-ARM/mAmeter.uvprojx` 直接 Build 即可。

> ⚠️ 源码为 **GBK 编码**，用 VSCode 等编辑器打开时请设置文件编码为 GBK，以免中文注释乱码。

## 📄 说明

- 仅包含源码与工程文件，不含编译产物（见 `.gitignore`）。
