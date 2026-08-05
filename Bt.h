#pragma once
#include <Arduino.h>

void Bt_Init();
void Bt_Loop();              // COM 模式读取
void Bt_ApplyVisibility();
void Bt_HandleLine(const String& line, bool fromUsb);
void Bt_SwitchModeAndRestart();   // 切换 BLE/COM 后重启生效
bool Bt_IsConnected();
bool Bt_IsDataActive();           // 最近是否收到上位机数据（断连/无数据判定）
