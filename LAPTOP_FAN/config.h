#pragma once

// ===================== 引脚 =====================
#define PIN_FAN      5
#define PIN_RELAY    19
#define PIN_BTN1     12
#define PIN_BTN2     13
#define PIN_BTN3     14
#define PIN_BTN4     15
#define PIN_BTN5     16
#define BUTTON_COUNT 5

// ===================== 风扇 =====================
#define LEDC_CHANNEL    0
#define LEDC_RESOLUTION 8
#define DEFAULT_FAN_FREQ 15000
#define FAN_FREQ_MIN    1000
#define FAN_FREQ_MAX    40000
#define FAN_FREQ_STEP   1000

// ===================== 显示 =====================
#define OLED_ADDR       0x3C
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define STARTUP_MS      2000
#define BRIGHTNESS_DEFAULT 10
#define BRIGHTNESS_MIN  1
#define BRIGHTNESS_MAX  10

// ===================== 时间 =====================
// 外部校时最小间隔（分钟）：期间上位机 TIME 指令被忽略，由本地时钟继续走时
#define TIME_CALIB_INTERVAL_MIN 10

// ===================== 按键（对齐原固件） =====================
#define DEBOUNCE_MS      20
#define LONG_PRESS_MS    500
#define RAPID_PRESS_MS   100
#define SLEEP_PRESS_MS   2000
#define WAKEUP_PROTECT_MS 500

// ===================== 菜单 =====================
#define MENU_ITEM_COUNT  4

// ===================== 蓝牙 =====================
#define BT_MODE_COM      0
#define BT_MODE_BLE      1

// 设备名：BLE 与经典蓝牙共用前缀，COM 模式自动加 -COM 后缀。
// 可在编译期覆盖（烧录器用），例如：
//   arduino-cli compile ... --build-property compiler.cpp.extra_flags=-DBT_DEVICE_NAME=\"MY-FAN\"
#ifndef BT_DEVICE_NAME
#define BT_DEVICE_NAME "LAPTOP-FAN"
#endif
#ifndef BT_DEVICE_NAME_COM
#define BT_DEVICE_NAME_COM BT_DEVICE_NAME "-COM"
#endif

// ===================== 其他 =====================
#define DISPLAY_REFRESH_MS 500
#define SAVE_DELAY_MS      60000
