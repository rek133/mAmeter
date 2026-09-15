#include "zero_calibration.h"
/* ===== 回零步数 = 名义回零量 + 偏移修正 =====
   允许负数: 负 = 反方向走 |值| 步(就近回零);
   仅当 |值| 超过 MAX 才判为偏移损坏 → 用默认回零量并丢弃偏移 */
#define REVERSE_INT_DEFAULT   ((int32_t)(INTEGER_STEPS_PER_DEGREE * 45))     /* 整数盘名义回零(约400步, 45度) */
#define REVERSE_INT_MAX       800
#define REVERSE_FRAC_DEFAULT  480    /* 小数盘名义回零步数(用户先给的正数占位, 待实测) */
#define REVERSE_FRAC_MAX      1000


/* 外部任务/队列句柄 */
extern osThreadId_t sensorTaskHandle;
extern osThreadId_t stepperTaskHandle;
extern osThreadId_t defaultTaskHandle;
extern osMessageQueueId_t keyEventQueueHandle;

/* 从Flash读取参数; 越界或未写入(0xFFFF)时返回默认值 */
static int32_t Flash_Read_Param(uint32_t addr, uint32_t defaultValue, int32_t min, int32_t max)
{
    int32_t value = *(int32_t *)addr;
    return (value >= min && value <= max) ? value : (int32_t)defaultValue;
}

/* 擦除参数页(四个参数同页, 写入前必须整页擦除) */
static void Flash_Erase(void)
{
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError;

    HAL_FLASH_Unlock();
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = BAUD_RATE_FLASH_ADDR;   /* page base = first param address */
    eraseInit.NbPages = 1;
    HAL_FLASHEx_Erase(&eraseInit, &sectorError);
    HAL_FLASH_Lock();
}

/* ===== 保存: 整页擦除后一次写回四个参数(共用) ===== */
static void Flash_Write_All(int32_t zeroOffset, int32_t fracOffset,
                            uint32_t headingType, uint32_t baudRate)
{
    Flash_Erase();
    HAL_FLASH_Unlock();
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ZERO_OFFSET_FLASH_ADDR, (uint32_t)zeroOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FRACTION_OFFSET_FLASH_ADDR, (uint32_t)fracOffset);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, HEADING_TYPE_FLASH_ADDR, headingType);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BAUD_RATE_FLASH_ADDR, baudRate);
    HAL_FLASH_Lock();
}

/* 各保存入口: 先读同页其余参数再整页重写, 保证4参数同页不丢 */
void Flash_Save_ZeroOffset(void)
{
    Flash_Write_All(sysState.zeroOffset,
                    Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000),
                    Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6),
                    Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400));
}

void Flash_Save_FractionZeroOffset(void)
{
    Flash_Write_All(Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000),
                    sysState.fractionZeroOffset,
                    Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6),
                    Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400));
}

void Flash_Save_HeadingType(uint8_t headingType)
{
    Flash_Write_All(Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000),
                    Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000),
                    headingType,
                    Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400));
}

void Flash_Save_BaudRate(uint32_t baudRate)
{
    Flash_Write_All(Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000),
                    Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000),
                    Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6),
                    baudRate);
}

/* ===== 加载 ===== */
void Flash_Load_ZeroOffset(void)
{
    sysState.zeroOffset = Flash_Read_Param(ZERO_OFFSET_FLASH_ADDR, DEFAULT_ZERO_OFFSET, -10000, 10000);
}

void Flash_Load_FractionZeroOffset(void)
{
    sysState.fractionZeroOffset = Flash_Read_Param(FRACTION_OFFSET_FLASH_ADDR, DEFAULT_FRACTION_ZERO_OFFSET, -10000, 10000);
}

void Flash_Load_HeadingType(void)
{
    sysState.currentHeadingType = Flash_Read_Param(HEADING_TYPE_FLASH_ADDR, DEFAULT_HEADING_TYPE, 0, 6);
}

void Flash_Load_BaudRate(void)
{
    /* 波特率只接受白名单值 */
    uint32_t baud = Flash_Read_Param(BAUD_RATE_FLASH_ADDR, DEFAULT_BAUD_RATE, 4800, 38400);
    sysState.currentBaudRate = (baud == 4800 || baud == 9600 || baud == 38400) ? baud : DEFAULT_BAUD_RATE;
}

/* 零位传感器消抖: 连续3次读到低电平才确认触发 */
static uint8_t sensorDebounce(void)
{
    static uint8_t debounceCnt = 0;
    uint8_t currentState = HAL_GPIO_ReadPin(ZERO_SENSOR_PRES_PORT, ZERO_SENSOR_PRES_PIN);

    if (currentState == 0) {
        if (++debounceCnt >= 3) {
            debounceCnt = 3;
            return 0;
        }
    } else {
        debounceCnt = 0;
    }
    return 1;
}

static void zeroingPrepare(uint8_t zeroingType); /* 前向声明(超时重试用) */

/* 调零整体完成: 关驱动, 恢复采集/步进任务, 挂起本状态机 */
static void zeroingComplete(void)
{
    HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_SET);
    sysState.zeroingStage = 2;
    menu_ctrl.current_status = MENU_IDLE;
    sysState.is_in_menu = 0;
    sysState.is_in_zeroing = 0;
    /* 调零后让显示值/目标与逻辑位置同步, 防止数码管滞留旧值、目标锁死在旧位置 */
    sysState.displayedDutyCycle = sysState.currentDutyCycle;
    sysState.targetDutyCycle    = sysState.currentDutyCycle;
    delay_us(2000);
    osDelay(1000);
    if (sensorTaskHandle != NULL) osThreadResume(sensorTaskHandle);
    if (stepperTaskHandle != NULL) osThreadResume(stepperTaskHandle);
    osThreadSuspend(defaultTaskHandle); /* 挂起自己, 等菜单/开机再次调用 */
}

/*
 * 调零状态机 (由 defaultTask 每1ms调用)
 *   stage 0: 整数盘调零(含手动微调)
 *   stage 1: 小数盘调零(自动/手动都要搜), 完成后整体结束
 *   stage 3: 进入调零时传感器已被挡 -> 先让小数盘离开感应区
 */

void zeroingStateMachine(void)
{
    static uint32_t lastStepTime = 0;
    static uint32_t fractionLastStepTime = 0;
    uint32_t currentTime = osKernelGetTickCount();
    uint8_t presence = sensorDebounce();
    uint8_t timed_out;
    KeyEvent keyEvent = KEY_EVENT_NONE;

    osMessageQueueGet(keyEventQueueHandle, &keyEvent, 0, 0); /* 非阻塞取按键 */
    /* 探测超时标志(微调模式每拍刷新zeroing_timeout, 不受影响) */
    timed_out = (uint32_t)(currentTime - sysState.zeroing_timeout) > ZEROING_TIMEOUT_MS;

    /* stage 3: 离开感应区后再从头开始(慢速, 避免起步过快) */
    if (sysState.zeroingStage == 3) {
        if (currentTime - fractionLastStepTime > 16 && sysState.fractionReverse90Steps > 0) {
            setFractionMotorDirection(0);
            stepFractionMotor();
            fractionLastStepTime = currentTime;
            sysState.fractionReverse90Steps--;
        }
        if (sysState.fractionReverse90Steps == 0) {
            /* 与正常流程一致的90度回退量(约200步), 避免28800步(180度)半圈游荡 */
            sysState.fractionReverse90Steps = (int32_t)(INTEGER_STEPS_PER_DEGREE * 90 / 4);
            setFractionMotorDirection(1);
            sysState.zeroingStage = 0;
        }
        return;
    }

    /* ========== stage 0: 整数盘调零 ========== */
    if (sysState.zeroingStage == 0) {
        switch (sysState.zeroingMode) {
        case 0: /* 整数盘调零完成 -> 切换小数盘调零 */
            sysState.int_sensor_detected = 1;
            sysState.zeroingStage = 1;
            sysState.fractionZeroingMode = 1;
            sysState.currentDutyCycle = 0.0f;
            sysState.zeroing_timeout = osKernelGetTickCount();
            break;

        case 1: /* 转动整数盘直到传感器触发 */
            if (timed_out) { /* 超时未触发 -> 停机等Err/重试 */
                sysState.zeroingMode = 4;
                break;
            }
            if (currentTime - lastStepTime > 4) {
                stepIntegerMotor();
                lastStepTime = currentTime;
            }
            if (presence == 0) {
                sysState.zeroingMode = sysState.initialReverse90Done ? 2 : 5;
            }
            break;

        case 2: /* 触发后回退 reverseSteps 步: 回到0位(离开传感器方向) */
            if (currentTime - lastStepTime > 10 && sysState.reverseSteps != 0) {
                /* 2026-09-10 符号定方向: >0→dir0, <0→dir1(反向), 就近回零 */
                setIntegerMotorDirection(sysState.reverseSteps > 0 ? 0 : 1);
                stepIntegerMotor();
                lastStepTime = currentTime;
                if (sysState.reverseSteps > 0) sysState.reverseSteps--; else sysState.reverseSteps++;
            }
            if (sysState.reverseSteps == 0) {
                sysState.initialReverse90Done = 0;
                sysState.zeroingMode = (sysState.zeroingType == 1) ? 0 : 3;
                setIntegerMotorDirection(1);
            }
            break;

        case 3: /* 手动微调(仅手动调零进入): 短按1步/长按5步连发, 双键长按确认 */
            sysState.zeroing_timeout = osKernelGetTickCount();
            if (keyEvent == KEY_EVENT_BOTH_LONG) {
                sysState.zeroingMode = 0;
                /* 确认即提交: 无条件累加并落盘, 不再依赖 !=0 门槛 */
                sysState.zeroOffset += sysState.fineTuneSteps;
                Flash_Save_ZeroOffset();
                break;
            }
            switch (keyEvent) {
            case KEY_EVENT_UP_SHORT:  /* 短按: 1步 */
            case KEY_EVENT_UP_LONG:   /* 长按由按键层按节奏连发, 每事件1步, 平滑 */
                setIntegerMotorDirection(1);
                stepIntegerMotor();
                sysState.fineTuneSteps--;   /* 2026-09-10 修正: 原为++, 补偿方向反了 */
                break;
            case KEY_EVENT_DOWN_SHORT:
            case KEY_EVENT_DOWN_LONG:
                setIntegerMotorDirection(0);
                stepIntegerMotor();
                sysState.fineTuneSteps++;   /* 2026-09-10 修正: 原为-- */
                break;
            default:
                break;
            }
            break;

        case 4: /* 探测超时停机: 电机停, 双键长按重新调零 */
            if (keyEvent == KEY_EVENT_BOTH_LONG) {
                zeroingPrepare(sysState.zeroingType);
            }
            break;

        case 5: /* 首次触发后的90度回退, 以便二次找零 */
            if (currentTime - lastStepTime > 4 && sysState.reverse90Steps > 0) {
                setIntegerMotorDirection(0);
                stepIntegerMotor();
                lastStepTime = currentTime;
                sysState.reverse90Steps--;
            }
            if (sysState.reverse90Steps == 0) {
                sysState.initialReverse90Done = 1;
                setIntegerMotorDirection(1);
                sysState.zeroingMode = 1;
            }
            break;
        }
    }
    /* ========== stage 1: 小数盘调零 ========== */
    else if (sysState.zeroingStage == 1) {
        switch (sysState.fractionZeroingMode) {
        case 0: /* 小数盘完成 -> 整体调零结束, 恢复运行任务 */
            sysState.frac_sensor_detected = 1;
            zeroingComplete();
            break;

        case 1: /* 转动小数盘直到传感器触发 */
            if (timed_out) { /* 超时未触发 -> 停机等Err/重试 */
                sysState.fractionZeroingMode = 4;
                break;
            }
            if (currentTime - fractionLastStepTime > 16) {
                stepFractionMotor();
                fractionLastStepTime = currentTime;
            }
            if (presence == 0) {
                sysState.fractionZeroingMode = sysState.fractionInitialReverse90Done ? 2 : 5;
            }
            break;

        case 2: /* 触发后回退 fractionReverseSteps 步 */
            if (currentTime - fractionLastStepTime > 40 && sysState.fractionReverseSteps != 0) {
                /* 2026-09-10 符号定方向: >0→dir0, <0→dir1(反向), 就近回零 */
                setFractionMotorDirection(sysState.fractionReverseSteps > 0 ? 0 : 1);
                stepFractionMotor();
                fractionLastStepTime = currentTime;
                if (sysState.fractionReverseSteps > 0) sysState.fractionReverseSteps--; else sysState.fractionReverseSteps++;
            }
            if (sysState.fractionReverseSteps == 0) {
                sysState.fractionInitialReverse90Done = 0;
                sysState.fractionZeroingMode = (sysState.zeroingType == 1) ? 0 : 3;
                setFractionMotorDirection(1);
            }
            break;

        case 3: /* 手动微调: 短按1步/长按5步连发, 双键长按确认 */
            sysState.zeroing_timeout = osKernelGetTickCount();
            if (keyEvent == KEY_EVENT_BOTH_LONG) {
                sysState.fractionZeroingMode = 0;
                /* 确认即提交: 无条件累加并落盘 */
                sysState.fractionZeroOffset += sysState.fractionFineTuneSteps;
                Flash_Save_FractionZeroOffset();
                break;
            }
            switch (keyEvent) {
            case KEY_EVENT_UP_SHORT:  /* 短按: 1步 */
            case KEY_EVENT_UP_LONG:   /* 长按按节奏连发, 每事件1步, 平滑 */
                setFractionMotorDirection(1);
                stepFractionMotor();
                sysState.fractionFineTuneSteps--;   /* 2026-09-10 修正: 原为++, 补偿方向反了 */
                break;
            case KEY_EVENT_DOWN_SHORT:
            case KEY_EVENT_DOWN_LONG:
                setFractionMotorDirection(0);
                stepFractionMotor();
                sysState.fractionFineTuneSteps++;   /* 2026-09-10 修正: 原为-- */
                break;
            default:
                break;
            }
            break;

        case 4: /* 探测超时停机: 电机停, 双键长按重新调零 */
            if (keyEvent == KEY_EVENT_BOTH_LONG) {
                zeroingPrepare(sysState.zeroingType);
            }
            break;

        case 5: /* 首次触发后的90度回退, 以便二次找零 */
            if (currentTime - fractionLastStepTime > 16 && sysState.fractionReverse90Steps > 0) {
                setFractionMotorDirection(0);
                stepFractionMotor();
                fractionLastStepTime = currentTime;
                sysState.fractionReverse90Steps--;
            }
            if (sysState.fractionReverse90Steps == 0) {
                sysState.fractionInitialReverse90Done = 1;
                setFractionMotorDirection(1);
                sysState.fractionZeroingMode = 1;
            }
            break;
        }
    }
}

/* 共用初始化: 重置调零相关状态并启动 */
static void zeroingPrepare(uint8_t zeroingType)
{
    uint8_t presence0;

    HAL_GPIO_WritePin(EN_PORT, EN_PIN, GPIO_PIN_RESET); /* 使能驱动 */
    presence0 = HAL_GPIO_ReadPin(ZERO_SENSOR_PRES_PORT, ZERO_SENSOR_PRES_PIN);

    sysState.zeroingMode = 1;
    sysState.zeroingType = zeroingType;
    sysState.uartDataReady = 0;
    sysState.is_in_zeroing = 1;
    sysState.zeroingStage = 0;
    if (presence0 == 0) {
        sysState.zeroingStage = 3; /* 传感器已被挡, 先离开感应区 */
    }
    sysState.int_sensor_detected = 0;
    sysState.frac_sensor_detected = 0;
    sysState.zeroing_timeout = osKernelGetTickCount();

    /* 整数盘参数 */
    sysState.lastDetectTime = osKernelGetTickCount();
    sysState.fineTuneSteps = 0;
    {
        /* 回零步数 = 名义量(45度) + 偏移修正. 偏移损坏/超范围时:
           丢弃该偏移(置0), 改用默认回零步数, 并置提示标志(不静默置0) */
        int32_t rs = REVERSE_INT_DEFAULT + sysState.zeroOffset;
        sysState.zeroOffsetInvalid = 0;
        /* 允许负数: 负 = 反方向走 |rs| 步(就近回零); 只在幅度离谱时才判非法 */
        if (rs > REVERSE_INT_MAX || rs < -REVERSE_INT_MAX) {
            sysState.zeroOffset = 0;
            rs = REVERSE_INT_DEFAULT;
            sysState.zeroOffsetInvalid = 1;
        }
        sysState.reverseSteps = rs;
    }
    sysState.reverse90Steps = (int32_t)(INTEGER_STEPS_PER_DEGREE * 90);
    setIntegerMotorDirection(1);

    /* 小数盘参数 */
    sysState.fractionZeroingMode = 0;
    {
        int32_t frs = REVERSE_FRAC_DEFAULT + sysState.fractionZeroOffset;
        sysState.fractionOffsetInvalid = 0;
        /* 允许负数: 负 = 反方向走 |frs| 步(就近回零) */
        if (frs > REVERSE_FRAC_MAX || frs < -REVERSE_FRAC_MAX) {
            sysState.fractionZeroOffset = 0;
            frs = REVERSE_FRAC_DEFAULT;
            sysState.fractionOffsetInvalid = 1;
        }
        sysState.fractionReverseSteps = frs;
    }
    sysState.fractionReverse90Steps = (int32_t)(INTEGER_STEPS_PER_DEGREE * 90 / 4);
    sysState.fractionFineTuneSteps = 0;
    if (zeroingType == 1) {
        sysState.fractionInitialReverse90Done = 0;
        setFractionMotorDirection(1);
    }

    KeyManager_Init();
}

/* 自动调零(上电/退出菜单时调用) */
void enterAutoZeroingMode(void)
{
    zeroingPrepare(1);
}

/* 手动调零(菜单选择进入) */
void enterZeroingMode(void)
{
    osThreadResume(defaultTaskHandle); /* 先恢复调零任务 */
    zeroingPrepare(0);
}
