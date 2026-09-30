#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "FanControl.h"
#include "Menu.h"
#include "Display.h"
#include "Buttons.h"

// B1=菜单 B2=下翻/自动 B3=上翻/手动 B4=增加/转速+ B5=减少/转速-
const uint8_t BUTTON_PINS[BUTTON_COUNT] = {PIN_BTN1, PIN_BTN2, PIN_BTN3, PIN_BTN4, PIN_BTN5};

bool buttonStates[BUTTON_COUNT] = {false};
unsigned long buttonPressTimes[BUTTON_COUNT] = {0};
unsigned long lastRapidAdjustTime[BUTTON_COUNT] = {0};
bool pressCandidate[BUTTON_COUNT] = {false};
unsigned long pressCandidateTime[BUTTON_COUNT] = {0};

bool wakeupProtection = false;
unsigned long wakeupTime = 0;

static void enterSleepMode();
static void exitSleepMode();
static void onButtonPress(int i);
static void onButtonRepeat(int i);

void Buttons_Init()
{
  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    pinMode(BUTTON_PINS[i], INPUT_PULLDOWN);
  }
  LOG_INFO("Buttons ready: %d keys", BUTTON_COUNT);
}

static void enterSleepMode()
{
  isSleeping = true;
  menuOpen = false;
  Fan_SetDuty(0);
  Display_Clear();
  Display_SetPanelOn(false);

  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    buttonStates[i] = false;
    buttonPressTimes[i] = 0;
    pressCandidate[i] = false;
  }
  LOG_INFO("Enter sleep");
}

static void exitSleepMode()
{
  isSleeping = false;
  Fan_SetDuty(manualDutyCycle);
  Display_SetPanelOn(true);
  wakeupProtection = true;
  wakeupTime = millis();

  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    buttonStates[i] = false;
    buttonPressTimes[i] = 0;
    pressCandidate[i] = false;
  }
  Display_RequestUpdate();
  LOG_INFO("Exit sleep");
}

static void onButtonPress(int i)
{
  if (isSleeping)
  {
    if (i == 0)
    {
      exitSleepMode();
    }
    return;
  }

  if (menuOpen)
  {
    switch (i)
    {
      case 0: Menu_Close(); break;
      case 1: Menu_Next(); break;
      case 2: Menu_Prev(); break;
      case 3: Menu_Adjust(1); break;
      case 4: Menu_Adjust(-1); break;
    }
    return;
  }

  switch (i)
  {
    case 0:
      Menu_Open();
      break;
    case 1:
      autoMode = true;
      Storage_MarkDirty();
      Display_RequestUpdate();
      LOG_INFO("Mode -> AUTO");
      break;
    case 2:
      autoMode = false;
      Fan_SetDuty(manualDutyCycle);
      Storage_MarkDirty();
      Display_RequestUpdate();
      LOG_INFO("Mode -> MANUAL");
      break;
    case 3:
      if (!autoMode)
      {
        manualDutyCycle = constrain(manualDutyCycle + 1, 0, 100);
        Fan_SetDuty(manualDutyCycle);
        Storage_MarkDirty();
      }
      break;
    case 4:
      if (!autoMode)
      {
        manualDutyCycle = constrain(manualDutyCycle - 1, 0, 100);
        Fan_SetDuty(manualDutyCycle);
        Storage_MarkDirty();
      }
      break;
  }
}

static void onButtonRepeat(int i)
{
  if (isSleeping || wakeupProtection)
  {
    return;
  }

  if (menuOpen)
  {
    Menu_Adjust(i == 3 ? 1 : -1);
  }
  else if (!autoMode)
  {
    manualDutyCycle = constrain(manualDutyCycle + (i == 3 ? 1 : -1), 0, 100);
    Fan_SetDuty(manualDutyCycle);
    Storage_MarkDirty();
  }
}

void Buttons_Loop(unsigned long now)
{
  if (wakeupProtection && (now - wakeupTime >= WAKEUP_PROTECT_MS))
  {
    wakeupProtection = false;
  }

  for (int i = 0; i < BUTTON_COUNT; i++)
  {
    bool pressed = digitalRead(BUTTON_PINS[i]) == HIGH;

    // 去抖：首次高电平后连续 DEBOUNCE_MS 稳定才确认
    if (pressed && !pressCandidate[i])
    {
      pressCandidate[i] = true;
      pressCandidateTime[i] = now;
    }
    else if (!pressed && pressCandidate[i])
    {
      pressCandidate[i] = false;
      buttonStates[i] = false;
      buttonPressTimes[i] = 0;
    }

    if (pressed && pressCandidate[i]
        && now - pressCandidateTime[i] >= DEBOUNCE_MS
        && !buttonStates[i])
    {
      // 按下沿：立即触发
      buttonStates[i] = true;
      buttonPressTimes[i] = now;
      lastRapidAdjustTime[i] = now;
      if (!wakeupProtection)
      {
        onButtonPress(i);
      }
    }

    // B1 长按 2 秒休眠
    if (buttonStates[0] && now - buttonPressTimes[0] >= SLEEP_PRESS_MS)
    {
      enterSleepMode();
      buttonStates[0] = false;
      pressCandidate[0] = false;
    }

    // B4/B5 长按连发（对齐原固件：500ms 后每 100ms）
    if (buttonStates[i]
        && (i == 3 || i == 4)
        && now - buttonPressTimes[i] >= LONG_PRESS_MS
        && now - lastRapidAdjustTime[i] >= RAPID_PRESS_MS)
    {
      onButtonRepeat(i);
      lastRapidAdjustTime[i] = now;
    }
  }
}
