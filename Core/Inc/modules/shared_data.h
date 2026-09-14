#ifndef SHARED_DATA_H
#define SHARED_DATA_H
#include "stm32f1xx_hal.h"
#include "cmsis_os2.h"  // 确保包含RTOS头文件


typedef struct {                     // 系统状态结构体
	
	  // ========== 小数电机状态 ==========
    int32_t fractionRemainingSteps;  // 小数电机剩余步数
    int8_t  fractionDirection;       // 小数电机方向 (1:顺时针, -1:逆时针)
    float fractionStepDelay;      // 小数电机步间延迟（ms）
    uint32_t lastFractionStepTime;   // 小数电机上次步进时间
	  int32_t fractionStepCounter; // 小数步进计数器（有符号，支持正反累积）
    // ========== 调零相关状态 ==========
		uint8_t zeroingStage;    // 0:整数调零中 1:小数调零中 2:调零完成
    uint8_t zeroingMode;             // 整数电机调零模式 
    int32_t reverseSteps;            /* 整数盘回退步数(有符号, 防负值回绕) */
    uint32_t lastDetectTime;         /* 上次检测到传感器信号的时间 */
    int32_t fineTuneSteps;           /* 整数盘微调累计步数 */
    int32_t zeroOffset;              // 整数电机零点偏移量
    uint8_t is_in_zeroing;           // 整体调零状态（1=调零中）
    uint8_t zeroingType;             /* 调零类型(1=自动, 0=手动) */
    uint8_t initialReverse90Done;    // 整数电机初始90度反转完成标志
    int32_t reverse90Steps;          /* 整数盘90度回退步数 */
    uint8_t int_sensor_detected;  /* 整数盘是否完成找零 */
    uint8_t frac_sensor_detected; /* 小数盘是否完成找零 */
    uint8_t zeroOffsetInvalid;        /* 整数回零偏移非法(已改用默认回零步数) */
    uint8_t fractionOffsetInvalid;    /* 小数回零偏移非法(已改用默认回零步数) */
    uint32_t zeroing_timeout;     // 调零超时计时
    #define ZEROING_TIMEOUT_MS 30000 // 调零超时时间（30秒, 原参数）
	
	  // ========== 小数电机调零相关 ==========
    uint8_t fractionZeroingMode;     // 小数电机调零模式
    int32_t fractionReverseSteps;    /* 小数盘回退步数(有符号) */
    int32_t fractionFineTuneSteps;   // 小数电机微调步数
    int32_t fractionZeroOffset;      // 小数电机零点偏移量
    uint8_t fractionInitialReverse90Done; // 小数电机初始90度反转完成标志
    int32_t fractionReverse90Steps;  /* 小数盘90度回退步数 */
		
		
    // ========== PWM输入处理 ==========
    float currentDutyCycle;          /* 小数盘当前位置(0.1度单位, 0~3600循环) */
    float targetDutyCycle;           /* 目标位置(0.1度单位) */
    
    uint8_t delayStableStepCnt;      // 当前延迟已走步数计数

		
    // ========== 串口输入相关 ==========
		uint32_t currentBaudRate;      // 当前波特率
    uint8_t currentHeadingType;    /* 航向语句类型索引 0~6 (HDG..EHDT) */
    float rotValue;                  /* NMEA解析出的航向值(度) */
    uint8_t uartDataReady;           // 串口数据就绪标志
		uint32_t uartDataLength;

    // ========== 菜单相关 ==========
    uint8_t is_in_menu;              // 菜单模式标志（1=进入菜单，0=退出菜单）
		
		// ========== 新增：显示相关状态 ==========
    uint8_t noDataFlag;        /* 无数据标记(1=超时无有效航向) */
		float displayedDutyCycle;  /* 跟随currentDutyCycle, 显示用(除以10得度数) */
		
    uint8_t decelStepCnt;
		uint8_t isMotorMoving; 

}SystemState;

extern SystemState sysState;
// 安全的时间差计算宏（解决uint32_t溢出问题）
#define TIME_DIFF(a, b)  ((uint32_t)((a) - (b)))

// ========== 步进电机基础物理参数（最核心，先定义） ==========
#define STEPS_PER_REVOLUTION    200         // 步进电机每转物理步数（1.8°/步）
#define MICROSTEPS              16          // 微步细分（0.225°/微步）

// ========== 步进电机每度步数（自动计算） ==========
#define INTEGER_STEPS_PER_DEGREE ((STEPS_PER_REVOLUTION * MICROSTEPS) / 360.0f)  // 整数电机每度步数（≈8.888步/度）
#define FRACTION_STEPS_PER_DEGREE 160                        // 小数电机每度步数（16步对应0.1°）

 
#define ROT_FILTER_WINDOW 5  // 滑动窗口大小（可根据效果调整，5~10合适）


// ========== 硬件引脚定义 ==========
#define BACKLIGHT_UP_PIN GPIO_PIN_2
#define BACKLIGHT_UP_PORT GPIOA
#define BACKLIGHT_DOWN_PIN GPIO_PIN_3
#define BACKLIGHT_DOWN_PORT GPIOA
										 
// TMC2225-SA 步进电机驱动引脚定义（整数电机）
#define MOTOR_INTEGER_DIR_PIN   GPIO_PIN_5     // 方向引脚
#define MOTOR_INTEGER_DIR_PORT  GPIOA
#define MOTOR_INTEGER_STEP_PIN  GPIO_PIN_6     // 步进引脚
#define MOTOR_INTEGER_STEP_PORT GPIOA

// TMC2225-SA 步进电机驱动引脚定义（小数电机）
#define MOTOR_FRACTION_DIR_PIN   GPIO_PIN_0     // 方向引脚
#define MOTOR_FRACTION_DIR_PORT  GPIOB
#define MOTOR_FRACTION_STEP_PIN  GPIO_PIN_1     // 步进引脚
#define MOTOR_FRACTION_STEP_PORT GPIOB

// 零位传感器引脚
#define ZERO_SENSOR_PRES_PIN   GPIO_PIN_4     // SENSOR
#define ZERO_SENSOR_PRES_PORT  GPIOB
#define EN_PIN  GPIO_PIN_5                    //  EN
#define EN_PORT GPIOB

// PWM输出引脚
#define PWM_PORT    GPIOA               
#define PWM_PIN     GPIO_PIN_8
//// 报警输出引脚定义
//#define ALARM_PIN        GPIO_PIN_12
//#define ALARM_PORT       GPIOB
//#define ALARM_ACTIVE     GPIO_PIN_SET   // 高电平报警
//#define ALARM_INACTIVE   GPIO_PIN_RESET // 低电平正常
#endif
