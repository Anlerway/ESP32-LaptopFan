#include <Arduino.h>
#include "config.h"
#include "State.h"

bool autoMode = true;
int dutyCycle = 45;
int manualDutyCycle = 45;
bool isSleeping = false;
bool menuOpen = false;
float cpuValue = 0.0f;
float gpuValue = 0.0f;
DateTime currentTime = {2026, 1, 1, 0, 0, 0};
bool timeSet = false;

// 本地时钟基于"锚点绝对秒 + millis 增量"派生，
// 外置校时只重设锚点，内部计时不再与校时值互相覆盖，避免秒数乱跳。
static unsigned long timeAnchorMillis = 0; // 锚点对应的 millis()
static long timeEpochSeconds = 0;          // 锚点对应的绝对秒（自 2000-01-01 00:00:00）
static unsigned long lastCalibMinutes = 0; // 上次校时对应的运行分钟数（millis()/60000）

static bool Time_IsLeapYear(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static const uint8_t timeDaysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static long Time_ToEpoch(const DateTime& t)
{
  long days = 0;
  for (int y = 2000; y < t.year; y++)
  {
    days += Time_IsLeapYear(y) ? 366 : 365;
  }
  for (int m = 1; m < t.month; m++)
  {
    days += timeDaysInMonth[m - 1];
    if (m == 2 && Time_IsLeapYear(t.year))
    {
      days++;
    }
  }
  days += t.day - 1;
  return days * 86400L + t.hour * 3600L + t.minute * 60L + t.second;
}

static DateTime Time_FromEpoch(long seconds)
{
  DateTime t;
  long days = seconds / 86400L;
  seconds %= 86400L;
  t.hour = (int)(seconds / 3600L);
  t.minute = (int)((seconds % 3600L) / 60L);
  t.second = (int)(seconds % 60L);

  t.year = 2000;
  while (days >= (Time_IsLeapYear(t.year) ? 366 : 365))
  {
    days -= Time_IsLeapYear(t.year) ? 366 : 365;
    t.year++;
  }
  t.month = 1;
  while (true)
  {
    int dim = timeDaysInMonth[t.month - 1];
    if (t.month == 2 && Time_IsLeapYear(t.year))
    {
      dim++;
    }
    if (days < dim)
    {
      break;
    }
    days -= dim;
    t.month++;
  }
  t.day = (int)days + 1;
  return t;
}

// 外置校时（上位机 TIME 指令）：重设锚点，本地时钟从此刻起由 millis 派生
void Time_SetExternal(const DateTime& t)
{
  unsigned long nowMinutes = millis() / 60000UL;
  // 间隔不足 TIME_CALIB_INTERVAL_MIN 分钟时忽略本次校时，
  // 本地时钟按上一锚点继续走时，避免频繁重置锚点。
  if (timeSet && (nowMinutes - lastCalibMinutes) < TIME_CALIB_INTERVAL_MIN)
  {
    return;
  }
  currentTime = t;
  timeEpochSeconds = Time_ToEpoch(t);
  timeAnchorMillis = millis();
  lastCalibMinutes = nowMinutes;
  timeSet = true;
}

// 取当前时间：锚点绝对秒 + millis 增量实时派生。
// 显示每帧刷新时调用，秒位只在真实秒边界变化，
// 校准时刻落在秒的哪个相位都不会造成跳秒/快慢不均。
DateTime Time_Now()
{
  if (timeAnchorMillis == 0)
  {
    timeEpochSeconds = Time_ToEpoch(currentTime);
    timeAnchorMillis = millis();
  }
  long elapsed = (long)((millis() - timeAnchorMillis) / 1000UL);
  return Time_FromEpoch(timeEpochSeconds + elapsed);
}

void Time_Tick()
{
  currentTime = Time_Now();
}
