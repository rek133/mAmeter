#ifndef MENU_ITEMS_H
#define MENU_ITEMS_H
#include "shared_data.h"
#include "cmsis_os.h"
#include "key_manager.h"

// 菜单状态枚举
typedef enum {
    MENU_IDLE,        // 空闲（非菜单模式）
    MENU_MAIN,        // 主菜单：-CF- / -||- / DATA
    MENU_CF_SUB,      // -CF-子菜单
    MENU_DATA_SUB,    // DATA子菜单
    MENU_ZEROING      // 调零子菜单（进入调零模式）
} MenuStatus;

// 菜单控制结构体
typedef struct {
    MenuStatus current_status; // 当前菜单状态
    uint8_t    main_index;     // 主菜单选中索引（0:-CF- / 1:-||- / 2:DATA）
	  uint8_t cf_index;      // -CF-子菜单索引
    uint8_t data_index;    // DATA子菜单索引
    uint8_t    self_check_done;// 自检完成标志
    // 闪烁相关字段
    uint32_t blink_start_time;
    uint32_t blink_duration;
    uint8_t blink_state;
} MenuCtrl;

extern MenuCtrl menu_ctrl;

// 菜单核心函数声明
void Menu_Init(void);
void Menu_Enter(void);     // 进入主菜单
void Menu_Exit(void);      // 退出菜单
void Menu_ProcessEvent(KeyEvent event); // 处理按键事件
void Menu_UpdateDisplay(void);          // 更新数码管显示
void Zeroing_Display_Process(void);

#endif
