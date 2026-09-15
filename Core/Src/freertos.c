#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "pwm_input.h"
#include "stepper_motor.h"
#include "backlight.h"
#include "zero_calibration.h"
#include "shared_data.h"
#include "usart.h"
#include "key_manager.h"
#include "menu_items.h"
#include "watchdog.h"
#include <math.h> 

osMessageQueueId_t keyEventQueueHandle;
#define KEY_QUEUE_SIZE 5  // 队列大小，足够缓存按键事件
#define NO_DATA_TIMEOUT 2000  /* 无有效数据超过2s判定断线 */

osThreadId_t defaultTaskHandle;
osThreadId_t sensorTaskHandle;
osThreadId_t stepperTaskHandle;
osThreadId_t buttonTaskHandle;


const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 2048,
  .priority = osPriorityNormal,   
};
const osThreadAttr_t stepperTask_attributes = {
  .name = "StepperTask",
  .stack_size = 256 * 4,
  .priority = osPriorityAboveNormal,          
};
const osThreadAttr_t buttonTask_attributes = {
  .name = "ButtonTask",
  .stack_size = 256 * 4,
  .priority = osPriorityLow,       
};
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = osPriorityHigh,          
};

void Task_Sensor(void *argument);
void Task_Stepper(void *argument);
void Task_Button(void *argument);
void StartDefaultTask(void *argument);
extern void processReceivedData(uint16_t len);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */


void MX_FREERTOS_Init(void) {
  keyEventQueueHandle = osMessageQueueNew(KEY_QUEUE_SIZE, sizeof(KeyEvent), NULL);

    sensorTaskHandle = osThreadNew(Task_Sensor, NULL, &sensorTask_attributes);
  stepperTaskHandle = osThreadNew(Task_Stepper, NULL, &stepperTask_attributes);
  buttonTaskHandle = osThreadNew(Task_Button, NULL, &buttonTask_attributes);
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
    
  /* 开机先挂起采集/步进任务, 只跑调零状态机+按键 */
if (sensorTaskHandle != NULL) osThreadSuspend(sensorTaskHandle);
if (stepperTaskHandle != NULL) osThreadSuspend(stepperTaskHandle);
  enterAutoZeroingMode();
}
void StartDefaultTask(void *argument)
{

  for(;;)
  {
         zeroingStateMachine();      //调零状态机
         osDelay(1);
  }
} 
void Task_Sensor(void *argument)
{
    static float filteredValue = 0;
    static float rotBuffer[ROT_FILTER_WINDOW] = {0};
    static uint8_t bufferIndex = 0;
    static uint8_t validCount = 0;
    static uint32_t lastValidDataTime = 0;

    for (;;) {
        processReceivedData(sysState.uartDataLength);

        if (sysState.uartDataReady) {
            float sum = 0.0f, avgRot;
            uint8_t i;

            lastValidDataTime = osKernelGetTickCount();
            rotBuffer[bufferIndex] = sysState.rotValue;
            bufferIndex = (bufferIndex + 1) % ROT_FILTER_WINDOW;
            if (validCount < ROT_FILTER_WINDOW) {
                validCount++;
            }
            for (i = 0; i < validCount; i++) {
                sum += rotBuffer[i];
            }
            avgRot = sum / validCount;
            filteredValue = roundf(avgRot * 10.0f);

            /* 与上一个目标比较(而非显示值), 做360度环绕差; 变化>=0.5度才更新目标, 抑制抖动 */
            {
                float diff = fabsf(filteredValue - sysState.targetDutyCycle);
                if (diff > 1800.0f) diff = 3600.0f - diff;
                if (diff >= 5.0f) sysState.targetDutyCycle = filteredValue;
            }
            sysState.uartDataReady = 0;   /* 消费掉本帧, 防止同一帧被反复处理 */
            sysState.noDataFlag = 0;
        } else if (osKernelGetTickCount() - lastValidDataTime > NO_DATA_TIMEOUT) {
            sysState.noDataFlag = 1;
        }
        osDelay(20);
    }
}


void Task_Stepper(void *argument) {

  for(;;) {

    moveMotorTask();

        osDelay(1);
  }
}
void Task_Button(void *argument)
{
    KeyManager_Init();
    Menu_Init();
    uint8_t dispCode[4] = {20, 20, 20, 20};

    for (;;) {
        Watchdog_Feed();   /* 喂狗: 本任务最低优先级且永不挂起 */
        KeyEvent event = KeyManager_Process();

        if (sysState.is_in_zeroing) {
            /* 调零中: 按键事件进队列交给调零状态机, 并刷新调零过程显示 */
            if (event != KEY_EVENT_NONE) {
                osMessageQueuePut(keyEventQueueHandle, &event, 0, 0);
            }
            Zeroing_Display_Process();
        } else {
            /* 正常/菜单: 事件直接处理, 并刷新菜单显示 */
            Menu_ProcessEvent(event);
            Menu_UpdateDisplay();
        }

        /* 数值显示: 无数据2s后显示 -99 */
        if (sysState.noDataFlag) {
            convertToDisplayCode(-99.0f, dispCode);
        } else {
            convertToDisplayCode(sysState.displayedDutyCycle / 10.0f, dispCode);
        }
        if (menu_ctrl.current_status == MENU_IDLE && !sysState.is_in_zeroing) {
            TM1637_display(dispCode[0], dispCode[1], dispCode[2], dispCode[3]);
        }
        osDelay(50);
    }
}


