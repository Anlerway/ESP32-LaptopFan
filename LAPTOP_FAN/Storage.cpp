#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include <Preferences.h>

Preferences preferences;
int btMode = BT_MODE_COM;
bool btVisible = false;
int brightness = BRIGHTNESS_DEFAULT;
int fanFrequency = DEFAULT_FAN_FREQ;
bool savePending = false;
unsigned long lastSaveTime = 0;

void Storage_Init()
{
  preferences.begin("fan-controller", false);
}

void Storage_Load()
{
  autoMode = preferences.getBool("auto_mode", true);
  manualDutyCycle = preferences.getInt("manual_duty", 45);
  btMode = preferences.getInt("bt_mode", BT_MODE_COM);
  btVisible = preferences.getBool("bt_visible", false);
  brightness = preferences.getInt("brightness", BRIGHTNESS_DEFAULT);
  fanFrequency = preferences.getInt("fan_freq", DEFAULT_FAN_FREQ);

  dutyCycle = autoMode ? 0 : manualDutyCycle;

  LOG_INFO("Loaded: auto=%d duty=%d btMode=%s visible=%d bright=%d freq=%dHz",
           autoMode, manualDutyCycle, btMode == BT_MODE_BLE ? "BLE" : "COM",
           btVisible, brightness, fanFrequency);
}

void Storage_Save()
{
  preferences.putBool("auto_mode", autoMode);
  preferences.putInt("manual_duty", manualDutyCycle);
  preferences.putInt("bt_mode", btMode);
  preferences.putBool("bt_visible", btVisible);
  preferences.putInt("brightness", brightness);
  preferences.putInt("fan_freq", fanFrequency);
  savePending = false;
  lastSaveTime = millis();
  LOG_INFO("Settings saved");
}

void Storage_MarkDirty()
{
  savePending = true;
}

void Storage_Loop(unsigned long now)
{
  if (savePending && (now - lastSaveTime >= SAVE_DELAY_MS))
  {
    Storage_Save();
  }
}
