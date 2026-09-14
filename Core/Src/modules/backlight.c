#include "backlight.h"
#include "stepper_motor.h"
#include <stdlib.h>
#include <math.h>

/* 背光等级与TM1637亮度共用同一个等级值(0~4) */
static Backlight_State_t backlight = {
    .current_level = 2,
};

/* 背光等级 -> PWM占空比(CCR 0~999), 曲线近似非线性以改善观感 */
static const uint32_t brightness_map[BACKLIGHT_LEVELS] = {
    0,    /* 0: 关 */
    250,  /* 1 */
    500,  /* 2 */
    750,  /* 3 */
    999,  /* 4: 最亮 */
};

void Backlight_Init(void)
{
    HAL_TIM_PWM_Start(&BACKLIGHT_PWM_TIM_HANDLE, BACKLIGHT_PWM_CHANNEL);
#ifdef TIM1
    __HAL_TIM_MOE_ENABLE(&BACKLIGHT_PWM_TIM_HANDLE); /* 高级定时器主输出使能 */
#endif
    Backlight_SetLevel(backlight.current_level);
}

void Backlight_SetLevel(uint8_t level)
{
    if (level >= BACKLIGHT_LEVELS) {
        level = BACKLIGHT_LEVELS - 1;
    }
    backlight.current_level = level;
    __HAL_TIM_SET_COMPARE(&BACKLIGHT_PWM_TIM_HANDLE, BACKLIGHT_PWM_CHANNEL,
                          brightness_map[backlight.current_level]);
}

void Backlight_Increase(void)
{
    uint8_t new_level = backlight.current_level + 1;
    if (new_level >= BACKLIGHT_LEVELS) {
        new_level = BACKLIGHT_LEVELS; /* 上限, 由SetLevel钳位 */
    }
    Backlight_SetLevel(new_level);
    TM1637_SetBrightness(new_level);
}

void Backlight_Decrease(void)
{
    uint8_t new_level = (backlight.current_level == 0) ? 0 : backlight.current_level - 1;
    Backlight_SetLevel(new_level);
    TM1637_SetBrightness(new_level);
}

/*
 * 数值 -> 4位显示码(供TM1637_display使用)
 * 显示码含义: 0~9数字, 10~19数字带小数点, 20='-', 21=熄灭, 其余见tab[]自定义字符
 * 特殊值: value<=-99 时整屏显示 "----"(无数据)
 */
void convertToDisplayCode(float value, uint8_t *dispCode)
{
    float roundedValue;
    int32_t integerPart, hundredsPart, tensPart, unitsPart, decimalPart;
    uint8_t isNegative;

    if (dispCode == NULL) {
        return;
    }
    if (fabsf(value) < 0.05f) {
        value = 0.0f;
    }
    roundedValue = roundf((value + 1e-6f) * 10.0f) / 10.0f;

    if (value <= -99.0f) { /* 无数据标记 */
        dispCode[0] = 20;
        dispCode[1] = 20;
        dispCode[2] = 20;
        dispCode[3] = 20;
        return;
    }

    isNegative = (roundedValue < 0.0f);
    integerPart = (int32_t)fabsf(roundedValue);
    hundredsPart = (integerPart / 100) % 10;
    tensPart = (integerPart / 10) % 10;
    unitsPart = integerPart % 10;
    decimalPart = (int32_t)(roundedValue * 10.0f) % 10;
    decimalPart = abs(decimalPart);
    decimalPart = (decimalPart > 9) ? 9 : decimalPart;

    /* 高位补0(如45.6显示045.6); 第3/4位随后必定被覆盖 */
    dispCode[0] = 0;
    dispCode[1] = 0;
    dispCode[2] = 0;
    dispCode[3] = 0;

    dispCode[3] = decimalPart;
    dispCode[2] = 10 + unitsPart; /* 带小数点 */

    if (integerPart >= 10) {
        dispCode[1] = tensPart;
        if (hundredsPart > 0) {
            dispCode[0] = isNegative ? 20 : hundredsPart;
        }
    } else if (isNegative) {
        dispCode[1] = 20; /* 负数符号放在十位 */
    }
}

/* TM1637 段码表: 索引=显示码 */
static const uint8_t tab[] = {
    0x3F, /* 0  */
    0x06, /* 1  */
    0x5B, /* 2  */
    0x4F, /* 3  */
    0x66, /* 4  */
    0x6D, /* 5  */
    0x7D, /* 6  */
    0x07, /* 7  */
    0x7F, /* 8  */
    0x6F, /* 9  */
    0xBF, /* 0. */
    0x86, /* 1. */
    0xDB, /* 2. */
    0xCF, /* 3. */
    0xE6, /* 4. */
    0xED, /* 5. */
    0xFD, /* 6. */
    0x87, /* 7. */
    0xFF, /* 8. */
    0xEF, /* 9. */
    0x40, /* -  */
    0x00, /* 熄灭 */
    0x70, /* |- */
    0x46, /* -| */
    0x79, /* E  */
    0x50, /* r  */
    0x39, /* C  */
    0x71, /* F  */
    0x0F, /* ]  */
    0x5E, /* D  */
    0x77, /* A  */
    0x07, /* T  */
    0x76, /* H  */
    0x73, /* P  */
    0x6D, /* S  */
    0x3D, /* G  */
    0x37, /* M  */
};

static void TM1637_start(void)
{
    TM1637_CLK_HIGH();
    TM1637_DIO_HIGH();
    delay_us(2);
    TM1637_DIO_LOW();
}

static void TM1637_stop(void)
{
    TM1637_CLK_LOW();
    delay_us(2);
    TM1637_DIO_LOW();
    delay_us(2);
    TM1637_CLK_HIGH();
    delay_us(2);
    TM1637_DIO_HIGH();
    delay_us(2);
}

static void TM1637_write_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        TM1637_CLK_LOW();
        if (data & 0x01) {
            TM1637_DIO_HIGH();
        } else {
            TM1637_DIO_LOW();
        }
        delay_us(3);
        data >>= 1;
        TM1637_CLK_HIGH();
        delay_us(3);
    }
}

static void TM1637_ack(void)
{
    uint8_t i = 0;

    TM1637_CLK_LOW();
    delay_us(5);
    while (HAL_GPIO_ReadPin(TM1637_DIO_PORT, TM1637_DIO_PIN) == 0x01 && (i < 250)) {
        i++;
    }
    TM1637_CLK_HIGH();
    delay_us(2);
    TM1637_CLK_LOW();
}

/* 显示4位(段码索引) */
void TM1637_display(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    TM1637_start();
    TM1637_write_byte(0x40); /* 自动地址递增 */
    TM1637_ack();
    TM1637_stop();

    TM1637_start();
    TM1637_write_byte(0xC0); /* 从地址0开始 */
    TM1637_ack();
    TM1637_write_byte(tab[a]);
    TM1637_ack();
    TM1637_write_byte(tab[b]);
    TM1637_ack();
    TM1637_write_byte(tab[c]);
    TM1637_ack();
    TM1637_write_byte(tab[d]);
    TM1637_ack();
    TM1637_stop();
}

/* 设置TM1637亮度(0=关显示, 1~7对应0x88~0x8F) */
void TM1637_SetBrightness(uint8_t level)
{
    TM1637_start();
    if (level == 0) {
        TM1637_write_byte(0x80);
    } else {
        TM1637_write_byte(0x88 | ((level - 1) & 0x07));
    }
    TM1637_ack();
    TM1637_stop();
}
