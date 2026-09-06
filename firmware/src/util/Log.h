#pragma once
#include <stdarg.h>
#include <Arduino.h>

inline void logBoot(const char* tag, const char* fmt, ...) {
  Serial.print('[');
  Serial.print(tag);
  Serial.print("] ");
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}