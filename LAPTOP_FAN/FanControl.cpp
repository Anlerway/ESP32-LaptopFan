#include "config.h"
#include "debug.h"
#include "State.h"
#include "Storage.h"
#include "FanControl.h"

void Fan_Init()
{
  ledcSetup(LEDC_CHANNEL, fanFrequency, LEDC_RESOLUTION);
  ledcAttachPin(PIN_FAN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, map(dutyCycle, 0, 100, 0, 255));

  pinMode(PIN_RELAY, OUTPUT);
  Fan_UpdateRelay();
  LOG_INFO("Fan init: freq=%dHz duty=%d%%", fanFrequency, dutyCycle);
}

void Fan_SetDuty(int value)
{
  value = constrain(value, 0, 100);
  if (value != dutyCycle)
  {
    dutyCycle = value;
    ledcWrite(LEDC_CHANNEL, map(dutyCycle, 0, 100, 0, 255));
    Fan_UpdateRelay();
    LOG_DEBUG("Duty -> %d%%", dutyCycle);
  }
}

void Fan_UpdateRelay()
{
  digitalWrite(PIN_RELAY, (isSleeping || dutyCycle == 0) ? LOW : HIGH);
}

void Fan_ApplyFrequency()
{
  ledcSetup(LEDC_CHANNEL, fanFrequency, LEDC_RESOLUTION);
  ledcWrite(LEDC_CHANNEL, map(dutyCycle, 0, 100, 0, 255));
  LOG_INFO("Fan frequency -> %dHz", fanFrequency);
}
