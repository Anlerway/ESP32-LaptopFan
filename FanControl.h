#pragma once
#include <Arduino.h>

void Fan_Init();
void Fan_SetDuty(int value);
void Fan_UpdateRelay();
void Fan_ApplyFrequency();
