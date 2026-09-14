#ifndef KEY_MANAGER_H
#define KEY_MANAGER_H
#include "shared_data.h"
#include "cmsis_os.h"

// 先定义KeyEvent枚举（关键：放在最前面）
typedef enum {
    KEY_EVENT_NONE,
    KEY_EVENT_UP_SHORT,        // 上键短按
    KEY_EVENT_UP_LONG,         // 上键长按
    KEY_EVENT_DOWN_SHORT,      // 下键短按
    KEY_EVENT_DOWN_LONG,       // 下键长按
    KEY_EVENT_BOTH_SHORT,    // 新增：双键短按（确认）
    KEY_EVENT_BOTH_LONG,     // 修改：双键长按（进入菜单）
    KEY_EVENT_MENU_TIMEOUT     // 菜单超时退出
} KeyEvent;

// 按键管理函数
void KeyManager_Init(void);
void KeyManager_Reset(void);
KeyEvent KeyManager_Process(void);

// 外部函数声明（仅声明，不包含头文件）
extern void Backlight_Increase(void);
extern void Backlight_Decrease(void);
extern void enterZeroingMode(void);


#endif
