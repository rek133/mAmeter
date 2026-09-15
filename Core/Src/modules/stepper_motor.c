#include "stepper_motor.h"
#include "shared_data.h"
#include <stdlib.h>
#include <math.h>

/* DWT精确微秒延时(调用前需已运行main中的DWT_Init) */
void delay_us(uint32_t us)
{
    uint32_t start, cycles;

    if (us == 0) {
        return;
    }
    start = DWT->CYCCNT;
    cycles = (SystemCoreClock / 1000000) * us;
    while ((DWT->CYCCNT - start) < cycles);
}

/* ===== 整数盘电机 ===== */
void stepIntegerMotor(void)
{
    HAL_GPIO_WritePin(MOTOR_INTEGER_STEP_PORT, MOTOR_INTEGER_STEP_PIN, GPIO_PIN_SET);
    delay_us(200);
    HAL_GPIO_WritePin(MOTOR_INTEGER_STEP_PORT, MOTOR_INTEGER_STEP_PIN, GPIO_PIN_RESET);
    delay_us(200);
}

void setIntegerMotorDirection(uint8_t clockwise)
{
    HAL_GPIO_WritePin(MOTOR_INTEGER_DIR_PORT, MOTOR_INTEGER_DIR_PIN,
                      clockwise ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(100);
}

/* ===== 小数盘电机 ===== */
void stepFractionMotor(void)
{
    HAL_GPIO_WritePin(MOTOR_FRACTION_STEP_PORT, MOTOR_FRACTION_STEP_PIN, GPIO_PIN_SET);
    delay_us(200);
    HAL_GPIO_WritePin(MOTOR_FRACTION_STEP_PORT, MOTOR_FRACTION_STEP_PIN, GPIO_PIN_RESET);
    delay_us(200);
}

void setFractionMotorDirection(uint8_t clockwise)
{
    HAL_GPIO_WritePin(MOTOR_FRACTION_DIR_PORT, MOTOR_FRACTION_DIR_PIN,
                      clockwise ? GPIO_PIN_SET : GPIO_PIN_RESET);
    delay_us(100);
}

/* ===== 加减速控制参数 ===== */
#define FAST_DELAY          1.0f    /* 最快步间延时(ms) */
#define SLOW_DELAY          10.0f   /* 起步/收尾延时(ms) */
#define ACC_DEC_THRESHOLD   100.0f  /* 剩余距离阈值, 低于此值开始减速 */
#define START_STEP_NUM      30      /* 起步前30步强制慢速 */
#define ACC_STEP            0.1f    /* 加速: 每10步减0.1ms */
#define DEC_STEP            0.1f    /* 减速: 每10步加0.1ms */
#define DELAY_STABLE_STEPS  10      /* 每级延时保持的步数 */

/*
 * 小数盘移动控制: 目标/当前位置换算 -> 换向加减速 -> 步进 -> 位置累计
 * 整数盘由小数盘每9步联动1步(见下方计数器逻辑)
 */
void moveMotorTask(void)
{
    uint32_t currentTime = osKernelGetTickCount();
    uint32_t delay_ms = (uint32_t)roundf(sysState.fractionStepDelay);
    float rawDiff, dutyDifference, currentDistance;
    int8_t targetDirection;
    uint32_t targetRemainingSteps;

    /* ===== 1. 最短路径换算目标方向/剩余步数(位置环0~3600, 取<1800一侧) ===== */
    rawDiff = sysState.targetDutyCycle - sysState.currentDutyCycle;
    if (rawDiff > 1800.0f) {
        dutyDifference = rawDiff - 3600.0f;
    } else if (rawDiff < -1800.0f) {
        dutyDifference = rawDiff + 3600.0f;
    } else {
        dutyDifference = rawDiff;
    }
    targetDirection = (dutyDifference > 0) ? 1 : -1;
    targetRemainingSteps = abs((int32_t)(dutyDifference * 8));

    if (sysState.isMotorMoving == 0) {
        sysState.fractionRemainingSteps = targetRemainingSteps;
    }

    /* ===== 2. 目标方向与当前相反且仍有步数 -> 进入换向减速流程 ===== */
    if (sysState.isMotorMoving == 0 && sysState.fractionRemainingSteps > 0 &&
        targetDirection != sysState.fractionDirection) {
        sysState.isMotorMoving = 1;
        sysState.delayStableStepCnt = 0;
    }

    /* ===== 3. 统一步进执行(换向流程与正常加减速二选一) ===== */
    if ((sysState.isMotorMoving || sysState.fractionRemainingSteps > 0) &&
        TIME_DIFF(currentTime, sysState.lastFractionStepTime) >= delay_ms) {

        uint8_t reversing = 0;   /* 1 = 换向减速滑行中(此步不消耗目标剩余步数, 防死锁) */

        /* ---- 3.1 换向流程: 先减速停稳, 再换向并装载新目标 ---- */
        if (sysState.isMotorMoving) {
            if (sysState.fractionStepDelay < SLOW_DELAY) {
                sysState.fractionStepDelay += DEC_STEP;
                if (sysState.fractionStepDelay > SLOW_DELAY) {
                    sysState.fractionStepDelay = SLOW_DELAY;
                }
                reversing = 1;   /* 减速滑行阶段: 不扣剩余步数 */
            } else {
                sysState.isMotorMoving = 0;                     /* 退出换向流程 */
                sysState.fractionDirection = targetDirection;   /* 切换到新方向 */
                sysState.decelStepCnt = 0;
                sysState.fractionRemainingSteps = targetRemainingSteps;
                sysState.delayStableStepCnt = 0;
                /* DIR位与表盘读数方向相反, 取反映射: +命令转读数增大方向 */
                setFractionMotorDirection(sysState.fractionDirection != 1);
                setIntegerMotorDirection(sysState.fractionDirection != 1);
            }
        }
        /* ---- 3.2 正常移动: 起步慢速 -> 每10步加减0.1ms(靠近目标减速) ---- */
        else {
            currentDistance = fabsf(sysState.targetDutyCycle - sysState.currentDutyCycle);
            if (currentDistance > 1800.0f) {
                currentDistance = 3600.0f - currentDistance;
            }

            if (sysState.decelStepCnt < START_STEP_NUM) {
                sysState.fractionStepDelay = SLOW_DELAY;
                sysState.decelStepCnt++;
                sysState.delayStableStepCnt = 0;
            } else if (sysState.delayStableStepCnt >= DELAY_STABLE_STEPS) {
                if (currentDistance > ACC_DEC_THRESHOLD) {
                    if (sysState.fractionStepDelay > FAST_DELAY) {
                        sysState.fractionStepDelay -= ACC_STEP;
                        if (sysState.fractionStepDelay < FAST_DELAY) {
                            sysState.fractionStepDelay = FAST_DELAY;
                        }
                    }
                } else {
                    if (sysState.fractionStepDelay < SLOW_DELAY) {
                        sysState.fractionStepDelay += DEC_STEP;
                        if (sysState.fractionStepDelay > SLOW_DELAY) {
                            sysState.fractionStepDelay = SLOW_DELAY;
                        }
                    }
                }
                sysState.delayStableStepCnt = 0;
            } else {
                sysState.delayStableStepCnt++;
            }
        }

        /* 执行一步 */
        stepFractionMotor();
        if (!reversing) {
            sysState.fractionRemainingSteps--;
        }
        sysState.lastFractionStepTime = currentTime;

        /* 位置累计(0.125/步, 0~3600循环) */
        if (sysState.fractionDirection == 1) {
            sysState.currentDutyCycle += 0.125f;
            if (sysState.currentDutyCycle >= 3600.0f) {
                sysState.currentDutyCycle -= 3600.0f;
            }
        } else {
            sysState.currentDutyCycle -= 0.125f;
            if (sysState.currentDutyCycle < 0.0f) {
                sysState.currentDutyCycle += 3600.0f;
            }
        }
        sysState.displayedDutyCycle = sysState.currentDutyCycle;

        /* 整数盘联动: 小数盘每走9步, 整数盘走1步 */
        sysState.fractionStepCounter += (sysState.fractionDirection == 1) ? 1 : -1;
        if (sysState.fractionStepCounter > 18 || sysState.fractionStepCounter < -18) {
            sysState.fractionStepCounter = 0;
        }
        if (sysState.fractionStepCounter >= 9) {
            stepIntegerMotor();
            sysState.fractionStepCounter -= 9;
        } else if (sysState.fractionStepCounter <= -9) {
            stepIntegerMotor();
            sysState.fractionStepCounter += 9;
        }

        delay_ms = (uint32_t)roundf(sysState.fractionStepDelay);
    }
}
