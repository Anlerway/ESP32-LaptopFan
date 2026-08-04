#pragma once
#include <Arduino.h>

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1
#endif

#if DEBUG_ENABLED
#define LOG_INFO(...)  do { Serial.print("[INFO]  "); Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#define LOG_WARN(...)  do { Serial.print("[WARN]  "); Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#define LOG_ERR(...)   do { Serial.print("[ERROR] "); Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#define LOG_DEBUG(...) do { Serial.print("[DEBUG] "); Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#else
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERR(...)
#define LOG_DEBUG(...)
#endif
