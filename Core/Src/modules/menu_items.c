#include "menu_items.h"
#include "backlight.h"
#include "zero_calibration.h"

extern uint32_t menu_timeout;
extern void Reconfigure_UART_BaudRate(uint32_t baudRate);
extern osThreadId_t sensorTaskHandle;
extern osThreadId_t stepperTaskHandle;
extern osThreadId_t defaultTaskHandle;

/* 菜单控制实例 */
MenuCtrl menu_ctrl = {
    .current_status = MENU_IDLE,
    .main_index = 0,
    .cf_index = 0,
    .data_index = 0,
    .self_check_done = 0,
    .blink_start_time = 0,
    .blink_duration = 4000, /* 确认后的闪烁时长(ms); 0=进入子菜单的初始闪烁 */
    .blink_state = 0,
};

/*
 * 调零过程显示: 闪烁显示状态动画, 超时未检出则显示Err
 *   stage 0 -> 整数盘, 末位数字1; stage 1 -> 小数盘, 末位数字2
 */
void Zeroing_Display_Process(void)
{
    static uint32_t lastDisplayTime = 0;
    static uint8_t display_flag = 0;
    uint32_t currentTime = osKernelGetTickCount();
    uint8_t err = 0, digit = 1;

    if (!sysState.is_in_zeroing) {
        return;
    }
    if (currentTime - lastDisplayTime > 400) {
        display_flag = !display_flag;
        lastDisplayTime = currentTime;
    }

    if (sysState.zeroingStage == 0) {
        err = (currentTime - sysState.zeroing_timeout > ZEROING_TIMEOUT_MS) && !sysState.int_sensor_detected;
        digit = 1;
    } else if (sysState.zeroingStage == 1) {
        err = (currentTime - sysState.zeroing_timeout > ZEROING_TIMEOUT_MS) && !sysState.frac_sensor_detected;
        digit = 2;
        if (!err) {
            menu_ctrl.self_check_done = 1; /* 小数调零正常进行中, 视为自检通过 */
        }
    } else {
        return; /* stage 2/3 不更新显示 */
    }

    /* err: 'E r r x'; 正常: '- | - x'(闪烁动画) */
    TM1637_display(display_flag ? (err ? 24 : 20) : 21,
                   display_flag ? (err ? 25 : 23) : 21,
                   display_flag ? (err ? 25 : 22) : 21,
                   display_flag ? digit : 21);
}

void Menu_Init(void)
{
    menu_ctrl.current_status = MENU_IDLE;
}

/* 进入菜单: 挂起数据采集/步进任务 */
void Menu_Enter(void)
{
    if (sysState.is_in_zeroing || !menu_ctrl.self_check_done) {
        return;
    }
    osDelay(10);
    osThreadSuspend(sensorTaskHandle);
    osThreadSuspend(stepperTaskHandle);

    menu_ctrl.current_status = MENU_MAIN;
    menu_ctrl.main_index = 0;
    sysState.is_in_menu = 1;
    menu_timeout = osKernelGetTickCount();
    KeyManager_Reset();
}

/* 退出菜单: 重新执行自动调零 */
void Menu_Exit(void)
{
    osThreadResume(defaultTaskHandle); /* 恢复调零状态机任务 */
    enterAutoZeroingMode();
    osDelay(200);
    menu_ctrl.current_status = MENU_IDLE;
    sysState.is_in_menu = 0;
    TM1637_display(20, 20, 20, 20);
    KeyManager_Reset();
}

/* ---- 各菜单项的显示内容 ---- */
static void Draw_Main(void)
{
    switch (menu_ctrl.main_index) {
    case 0: TM1637_display(20, 26, 27, 20); break; /* -CF- */
    case 1: TM1637_display(26, 23, 22, 28); break; /* C|-|D */
    case 2: TM1637_display(29, 30, 31, 30); break; /* DATA */
    }
}

static void Draw_CF(void)
{
    switch (menu_ctrl.cf_index) {
    case 0: TM1637_display(20, 32, 29, 35); break; /* -HDG */
    case 1: TM1637_display(20, 32, 29, 31); break; /* -HDT */
    case 2: TM1637_display(20, 32, 29, 36); break; /* -HDM */
    case 3: TM1637_display(20, 31, 32, 34); break; /* -THS */
    case 4: TM1637_display(33, 32, 29, 31); break; /* PHDT */
    case 5: TM1637_display(26, 32, 29, 31); break; /* CHDT */
    case 6: TM1637_display(24, 32, 29, 31); break; /* EHDT */
    }
}

static void Draw_Data(void)
{
    switch (menu_ctrl.data_index) {
    case 0: TM1637_display(4, 8, 0, 0); break;   /* 4800 */
    case 1: TM1637_display(9, 6, 0, 0); break;   /* 9600 */
    case 2: TM1637_display(3, 8, 4, 0); break;   /* 38400 */
    }
}

static uint32_t BaudOfIndex(uint8_t idx)
{
    return (idx == 0) ? 4800 : (idx == 1) ? 9600 : 38400;
}

/* 选择项变动后的闪烁控制: 与当前生效值一致时恢复初始闪烁提示 */
static void CF_SelectionChanged(uint32_t now)
{
    if (menu_ctrl.cf_index == sysState.currentHeadingType) {
        menu_ctrl.blink_start_time = now;
        menu_ctrl.blink_state = 0;
        menu_ctrl.blink_duration = 0;
    } else {
        menu_ctrl.blink_start_time = 0;
    }
}

static void Data_SelectionChanged(uint32_t now)
{
    if (BaudOfIndex(menu_ctrl.data_index) == sysState.currentBaudRate) {
        menu_ctrl.blink_start_time = now;
        menu_ctrl.blink_state = 0;
        menu_ctrl.blink_duration = 0;
    } else {
        menu_ctrl.blink_start_time = 0;
    }
}

/*
 * 菜单按键处理
 * 闪烁状态: 进入子菜单时闪烁(blink_duration=0, 可继续选择);
 *           确认保存后闪烁4s(blink_duration=4000, 忽略按键), 到时自动退出
 */
void Menu_ProcessEvent(KeyEvent event)
{
    uint32_t now = osKernelGetTickCount();

    if (menu_ctrl.blink_start_time > 0) {
        uint8_t is_init_blink = (menu_ctrl.blink_duration == 4000) ? 0 : 1;

        if (is_init_blink) {
            /* 初始闪烁: 双键/超时直接退出 */
            if (event == KEY_EVENT_BOTH_LONG || event == KEY_EVENT_MENU_TIMEOUT) {
                menu_ctrl.blink_start_time = 0;
                Menu_Exit();
                return;
            }
        } else {
            /* 确认后闪烁: 忽略一切按键 */
            if (event == KEY_EVENT_BOTH_LONG || event == KEY_EVENT_MENU_TIMEOUT) {
                menu_ctrl.blink_start_time = 0;
                Menu_Exit();
            }
            return;
        }
    }

    if (!sysState.is_in_menu && menu_ctrl.current_status == MENU_IDLE) {
        return;
    }

    switch (menu_ctrl.current_status) {
    case MENU_MAIN:
        switch (event) {
        case KEY_EVENT_UP_SHORT:
            menu_ctrl.main_index = (menu_ctrl.main_index - 1 + 3) % 3;
            break;
        case KEY_EVENT_DOWN_SHORT:
            menu_ctrl.main_index = (menu_ctrl.main_index + 1) % 3;
            break;
        case KEY_EVENT_BOTH_SHORT:
            switch (menu_ctrl.main_index) {
            case 0: /* 进入 -CF- 子菜单 */
                menu_ctrl.current_status = MENU_CF_SUB;
                menu_ctrl.cf_index = sysState.currentHeadingType;
                menu_ctrl.blink_start_time = now;
                menu_ctrl.blink_state = 0;
                menu_ctrl.blink_duration = 0;
                break;
            case 1: /* 手动调零 */
                menu_ctrl.current_status = MENU_ZEROING;
                enterZeroingMode();
                break;
            case 2: /* 进入 DATA 子菜单 */
                menu_ctrl.current_status = MENU_DATA_SUB;
                menu_ctrl.data_index = (sysState.currentBaudRate == 4800) ? 0 :
                                       (sysState.currentBaudRate == 9600) ? 1 : 2;
                menu_ctrl.blink_start_time = now;
                menu_ctrl.blink_state = 0;
                menu_ctrl.blink_duration = 0;
                break;
            }
            break;
        case KEY_EVENT_MENU_TIMEOUT:
            Menu_Exit();
            break;
        default:
            break;
        }
        break;

    case MENU_CF_SUB:
        switch (event) {
        case KEY_EVENT_UP_SHORT:
            menu_ctrl.cf_index = (menu_ctrl.cf_index - 1 + 7) % 7;
            CF_SelectionChanged(now);
            break;
        case KEY_EVENT_DOWN_SHORT:
            menu_ctrl.cf_index = (menu_ctrl.cf_index + 1) % 7;
            CF_SelectionChanged(now);
            break;
        case KEY_EVENT_BOTH_SHORT:
            sysState.currentHeadingType = menu_ctrl.cf_index;
            Flash_Save_HeadingType(menu_ctrl.cf_index);
            menu_ctrl.blink_start_time = now;
            menu_ctrl.blink_state = 0;
            menu_ctrl.blink_duration = 4000; /* 确认: 闪烁4s后自动退出 */
            break;
        default:
            break;
        }
        break;

    case MENU_DATA_SUB:
        switch (event) {
        case KEY_EVENT_UP_SHORT:
            menu_ctrl.data_index = (menu_ctrl.data_index - 1 + 3) % 3;
            Data_SelectionChanged(now);
            break;
        case KEY_EVENT_DOWN_SHORT:
            menu_ctrl.data_index = (menu_ctrl.data_index + 1) % 3;
            Data_SelectionChanged(now);
            break;
        case KEY_EVENT_BOTH_SHORT:
            sysState.currentBaudRate = BaudOfIndex(menu_ctrl.data_index);
            Reconfigure_UART_BaudRate(sysState.currentBaudRate);
            Flash_Save_BaudRate(sysState.currentBaudRate);
            menu_ctrl.blink_start_time = now;
            menu_ctrl.blink_state = 0;
            menu_ctrl.blink_duration = 4000;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
}

/* 菜单显示刷新 */
void Menu_UpdateDisplay(void)
{
    uint32_t now;
    uint8_t blank;

    if (!menu_ctrl.self_check_done) {
        return;
    }
    now = osKernelGetTickCount();

    /* 闪烁: 半周期显示内容, 半周期熄灭 */
    if (menu_ctrl.blink_start_time > 0) {
        if (menu_ctrl.blink_duration != 0 &&
            (now - menu_ctrl.blink_start_time > menu_ctrl.blink_duration)) {
            menu_ctrl.blink_start_time = 0;
            Menu_Exit();
            return;
        }
        blank = ((now / 400) % 2 == menu_ctrl.blink_state) ? 0 : 1;
        if (blank) {
            TM1637_display(21, 21, 21, 21);
            return;
        }
        switch (menu_ctrl.current_status) {
        case MENU_CF_SUB:
            Draw_CF();
            break;
        case MENU_DATA_SUB:
            Draw_Data();
            break;
        default:
            TM1637_display(20, 20, 20, 20);
            break;
        }
        return;
    }

    /* 正常显示 */
    switch (menu_ctrl.current_status) {
    case MENU_MAIN:
        Draw_Main();
        break;
    case MENU_CF_SUB:
        Draw_CF();
        break;
    case MENU_DATA_SUB:
        Draw_Data();
        break;
    default:
        break;
    }
}
