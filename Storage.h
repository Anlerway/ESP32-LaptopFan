#pragma once
#include <Arduino.h>

// 设置项（全局，供各模块读取）
extern int btMode;       // BT_MODE_COM / BT_MODE_BLE
extern bool btVisible;   // 蓝牙显示/隐藏
extern int brightness;   // 1-10
extern int fanFrequency; // Hz
extern bool savePending;

void Storage_Init();
void Storage_Load();
void Storage_Save();
void Storage_MarkDirty();
void Storage_Loop(unsigned long now);
