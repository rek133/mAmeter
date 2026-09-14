#include "key_manager.h"
#include "backlight.h"
#include "menu_items.h"
#include "zero_calibration.h"
#include <stdbool.h>

/* ===== 按键状态(电平: 1=松开, 0=按下) ===== */
static uint8_t last_up_state = 1;
static uint8_t last_down_state = 1;
static uint32_t both_press_time = 0;
uint32_t menu_timeout = 0; /* 菜单活动时间戳(按键/进入时刷新) */
static uint32_t up_press_time = 0;
static uint32_t down_press_time = 0;
static bool up_pressed = false;
static bool down_pressed = false;
static bool up_long_triggered = false;
static bool down_long_triggered = false;
static uint32_t up_last_repeat_time = 0;
static uint32_t down_last_repeat_time = 0;
static bool both_long_triggered = false;
static bool menu_both_latched = false;   /* 菜单双键锁存: 本次按住是否已触发 */
static uint8_t key_swallow = 0;          /* 1=吞掉按键, 直到两键都松开 */

#define SHORT_MS       1000  /* 短按判定窗口(正常/菜单模式) */
#define ZERO_LONG_MS    4000  /* 调零模式单键长按阈值(原参数: 按住>=4s触发连发) */
#define BOTH_LONG_MS   4000  /* 双键长按(调零确认) */
#define MENU_BOTH_MS   1000  /* 双键长按(菜单确认) */
#define NORMAL_BOTH_MS 8000  /* 双键长按(正常模式进菜单) */
#define REPEAT_MS      40    /* 长按连发间隔 */

static void Key_Scan(uint8_t *up_state, uint8_t *down_state)
{
    *up_state = HAL_GPIO_ReadPin(BACKLIGHT_UP_PORT, BACKLIGHT_UP_PIN);
    *down_state = HAL_GPIO_ReadPin(BACKLIGHT_DOWN_PORT, BACKLIGHT_DOWN_PIN);
}

/*
 * 调零模式按键:
 *  - 单键: 松开判定, 按住<4s=短按(1步), >=4s=长按(每40ms连发, 每次5步)
 *  - 双键同按>=4s: BOTH_LONG(确认/退出)
 */
static KeyEvent Process_Zeroing_Mode(uint8_t up_state, uint8_t down_state)
{
    uint32_t currentTime = osKernelGetTickCount();
    KeyEvent event = KEY_EVENT_NONE;

    if (!up_state && !down_state) {
        /* 双键同按 */
        if (both_press_time == 0) {
            both_press_time = currentTime;
            both_long_triggered = false;
            up_pressed = false;    /* 屏蔽单键逻辑 */
            down_pressed = false;
        } else if (!both_long_triggered && (currentTime - both_press_time >= BOTH_LONG_MS)) {
            both_long_triggered = true;
            event = KEY_EVENT_BOTH_LONG;
        }
    } else {
        if (both_press_time > 0) {
            both_press_time = 0;
            both_long_triggered = false;
        }

        /* ---- 上键 ---- */
        if (last_up_state && !up_state) { /* 按下沿 */
            up_press_time = currentTime;
            up_pressed = true;
            up_long_triggered = false;
            up_last_repeat_time = currentTime;
        } else if (!last_up_state && up_state) { /* 释放沿 */
            if (up_pressed && !up_long_triggered) {
                uint32_t dur = currentTime - up_press_time;
                if (dur >= 100 && dur < ZERO_LONG_MS) {
                    event = KEY_EVENT_UP_SHORT;
                }
            }
            up_pressed = false;
            up_long_triggered = false;
        } else if (up_pressed) {
            uint32_t hold_duration = currentTime - up_press_time;
            if (hold_duration >= ZERO_LONG_MS) {
                if (!up_long_triggered) {
                    event = KEY_EVENT_UP_LONG;
                    up_long_triggered = true;
                    up_last_repeat_time = currentTime;
                } else if (currentTime - up_last_repeat_time >= REPEAT_MS) {
                    event = KEY_EVENT_UP_LONG;
                    up_last_repeat_time = currentTime;
                }
            }
        }

        /* ---- 下键(同上) ---- */
        if (last_down_state && !down_state) {
            down_press_time = currentTime;
            down_pressed = true;
            down_long_triggered = false;
            down_last_repeat_time = currentTime;
        } else if (!last_down_state && down_state) {
            if (down_pressed && !down_long_triggered) {
                uint32_t dur = currentTime - down_press_time;
                if (dur >= 100 && dur < ZERO_LONG_MS) {
                    event = KEY_EVENT_DOWN_SHORT;
                }
            }
            down_pressed = false;
            down_long_triggered = false;
        } else if (down_pressed) {
            uint32_t hold_duration = currentTime - down_press_time;
            if (hold_duration >= ZERO_LONG_MS) {
                if (!down_long_triggered) {
                    event = KEY_EVENT_DOWN_LONG;
                    down_long_triggered = true;
                    down_last_repeat_time = currentTime;
                } else if (currentTime - down_last_repeat_time >= REPEAT_MS) {
                    event = KEY_EVENT_DOWN_LONG;
                    down_last_repeat_time = currentTime;
                }
            }
        }
    }

    last_up_state = up_state;
    last_down_state = down_state;
    return event;
}

/*
 * 菜单模式按键:
 *  - 单键短按(100~1000ms): 移动菜单项
 *  - 双键同按>=1s: BOTH_SHORT(确认/返回)
 */
static KeyEvent Process_Menu_Mode(uint8_t up_state, uint8_t down_state)
{
    uint32_t currentTime = osKernelGetTickCount();
    KeyEvent event = KEY_EVENT_NONE;

    if (!up_state && !down_state) {
        /* 双键同按瞬间先清单键跟踪, 避免误触发 */
        last_up_state = 1;
        last_down_state = 1;
        up_press_time = 0;
        down_press_time = 0;

        if (both_press_time == 0) {
            both_press_time = currentTime;
            menu_both_latched = false;              /* 本轮双键锁存复位 */
        } else if (!menu_both_latched && (currentTime - both_press_time > MENU_BOTH_MS)) {
            menu_both_latched = true;               /* 锁存: 本次按住只触发一次 */
            menu_timeout = currentTime;
            event = KEY_EVENT_BOTH_SHORT;
        }
    } else {
        if (both_press_time > 0) {
            both_press_time = 0;
            menu_both_latched = false;              /* 松开才允许下次触发 */
        }

        /* 上键: 松开判定短按 */
        if (last_up_state && !up_state) {
            up_press_time = currentTime;
            last_up_state = up_state;
        } else if (!last_up_state && up_state) {
            uint32_t dur = currentTime - up_press_time;
            if (dur >= 100 && dur < SHORT_MS) {
                menu_timeout = currentTime;
                event = KEY_EVENT_UP_SHORT;
            }
            last_up_state = up_state;
        }

        /* 下键 */
        if (last_down_state && !down_state) {
            down_press_time = currentTime;
            last_down_state = down_state;
        } else if (!last_down_state && down_state) {
            uint32_t dur = currentTime - down_press_time;
            if (dur >= 100 && dur < SHORT_MS) {
                menu_timeout = currentTime;
                event = KEY_EVENT_DOWN_SHORT;
            }
            last_down_state = down_state;
        }
    }

    return event;
}

/*
 * 正常模式按键:
 *  - 单键: 松开时按<1s判定, 控制背光增/减
 *  - 双键同按>=8s: 进入菜单
 */
static KeyEvent Process_Normal_Mode(uint8_t up_state, uint8_t down_state)
{
    uint32_t currentTime = osKernelGetTickCount();

    if (!last_up_state && up_state) { /* 释放沿: 短按增亮 */
        if (currentTime - up_press_time < SHORT_MS) {
            Backlight_Increase();
        }
    }
    if (last_up_state && !up_state) { /* 按下沿: 记录时间 */
        up_press_time = currentTime;
    }

    if (!last_down_state && down_state) {
        if (currentTime - down_press_time < SHORT_MS) {
            Backlight_Decrease();
        }
    }
    if (last_down_state && !down_state) {
        down_press_time = currentTime;
    }

    if (!up_state && !down_state) {
        if (both_press_time == 0) {
            both_press_time = currentTime;
        } else if (currentTime - both_press_time > NORMAL_BOTH_MS) {
            if (!sysState.is_in_menu && !sysState.is_in_zeroing) {
                Menu_Enter();
                both_press_time = 0;
                last_up_state = up_state;
                last_down_state = down_state;
                return KEY_EVENT_BOTH_LONG;
            }
        }
    } else {
        both_press_time = 0;
    }

    last_up_state = up_state;
    last_down_state = down_state;
    return KEY_EVENT_NONE;
}

void KeyManager_Reset(void)
{
    last_up_state = 1;
    last_down_state = 1;
    both_press_time = 0;
    menu_timeout = 0;
    up_press_time = 0;
    down_press_time = 0;
    up_pressed = false;
    down_pressed = false;
    up_long_triggered = false;
    down_long_triggered = false;
    both_long_triggered = false;
    menu_both_latched = false;
    up_last_repeat_time = 0;
    down_last_repeat_time = 0;
    key_swallow = 1;
}

void KeyManager_Init(void)
{
    KeyManager_Reset();
}

KeyEvent KeyManager_Process(void)
{
    uint8_t up_state, down_state;

    Key_Scan(&up_state, &down_state);

    /* 模式刚切换: 吞掉事件, 直到两键都松开(防同一按被两个模式/菜单重复消费) */
    if (key_swallow) {
        if (up_state && down_state) {
            key_swallow = 0;
        }
        last_up_state = up_state;
        last_down_state = down_state;
        return KEY_EVENT_NONE;
    }

    if (sysState.is_in_menu && menu_ctrl.current_status != MENU_ZEROING) {
        return Process_Menu_Mode(up_state, down_state);
    }
    if (sysState.is_in_zeroing) {
        return Process_Zeroing_Mode(up_state, down_state);
    }
    return Process_Normal_Mode(up_state, down_state);
}
