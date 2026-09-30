#pragma once
#include <Arduino.h>

void Display_Init();
void Display_Startup();
void Display_SetBrightness(int level);
void Display_RequestUpdate();
void Display_Loop(unsigned long now);
void Display_ShowRestarting();
void Display_Clear();
void Display_SetPanelOn(bool on);
