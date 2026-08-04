#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "FanControl.h"
#include "Display.h"
#include "Menu.h"
#include "Bt.h"
#include "Buttons.h"

// LAPTOP-FAN 主程序：初始化各模块，主循环只做事件调度
void setup()
{
  Serial.begin(115200);
  delay(300);
  LOG_INFO("LAPTOP-FAN boot, core %d", xPortGetCoreID());

  Storage_Init();
  Storage_Load();

  Fan_Init();
  Display_Init();
  Display_Startup();       // 开机画面（含等待时间）
  Bt_Init();
  Buttons_Init();

  LOG_INFO("Setup done, btMode=%s", btMode == BT_MODE_BLE ? "BLE" : "COM");
}

void loop()
{
  unsigned long now = millis();
  static unsigned long lastTick = 0;
  delay(5); // 让出 CPU 空闲，降低功耗

  if (now - lastTick >= 1000)
  {
    lastTick = now;
    Time_Tick();  // 本地时钟走秒，TIME 指令负责校时
  }

  Bt_Loop();          // 接收蓝牙数据（COM 模式）
  Buttons_Loop(now);  // 按键扫描（去抖 + 按下即触发）
  Storage_Loop(now);  // 延迟保存
  Display_Loop(now);  // 限频刷新显示
}
