#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "FanControl.h"
#include "Display.h"
#include "Bt.h"
#include "Menu.h"

int menuSelection = 0;

void Menu_Init()
{
  menuOpen = false;
  menuSelection = 0;
}

void Menu_Open()
{
  menuOpen = true;
  menuSelection = 0;
  Display_RequestUpdate();
}

void Menu_Close()
{
  menuOpen = false;
  Display_RequestUpdate();
}

void Menu_Next()
{
  menuSelection = (menuSelection + 1) % MENU_ITEM_COUNT;
  Display_RequestUpdate();
}

void Menu_Prev()
{
  menuSelection = (menuSelection - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
  Display_RequestUpdate();
}

int Menu_Selection()
{
  return menuSelection;
}

void Menu_Adjust(int delta)
{
  switch (menuSelection)
  {
    case 0: // 蓝牙显示/隐藏
      btVisible = !btVisible;
      Bt_ApplyVisibility();
      Storage_MarkDirty();
      LOG_INFO("Menu: BT visible -> %s", btVisible ? "ON" : "OFF");
      break;

    case 1: // 蓝牙模式 COM/BLE
      Bt_SwitchModeAndRestart();
      break;

    case 2: // 亮度 1-10
      brightness = constrain(brightness + delta, BRIGHTNESS_MIN, BRIGHTNESS_MAX);
      Display_SetBrightness(brightness);
      Storage_MarkDirty();
      LOG_INFO("Menu: brightness -> %d", brightness);
      break;

    case 3: // 风扇频率
      fanFrequency = constrain(
        fanFrequency + delta * FAN_FREQ_STEP, FAN_FREQ_MIN, FAN_FREQ_MAX);
      Fan_ApplyFrequency();
      Storage_MarkDirty();
      break;
  }
  Display_RequestUpdate();
}
