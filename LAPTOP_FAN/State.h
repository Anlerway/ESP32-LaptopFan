#pragma once

// ===================== 全局运行状态（各模块共享） =====================
extern bool autoMode;        // true=自动（听上位机） false=手动
extern int dutyCycle;        // 0-100
extern int manualDutyCycle;  // 手动模式占空比
extern bool isSleeping;
extern bool menuOpen;
extern float cpuValue;
extern float gpuValue;
extern bool hasSensorData; // 是否收到过上位机温度/转速数据（无数据时界面显示 "--"）

struct DateTime
{
  int year, month, day, hour, minute, second;
};
extern DateTime currentTime;
extern bool timeSet;

/// <summary>本地时钟每秒走一格（上位机 TIME 指令负责校时）。</summary>
void Time_Tick();

/// <summary>外置校时（上位机 TIME 指令）：设置时间并重置本地时钟锚点，避免与内部计时冲突。</summary>
void Time_SetExternal(const DateTime& t);

/// <summary>取当前时间：按锚点 + millis 增量实时派生，供显示刷新时调用。</summary>
DateTime Time_Now();
