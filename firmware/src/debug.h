#pragma once
#include <Arduino.h>
#include "config.h"

// Optional logging on the USB CDC port. TX only: never call Serial.read() – the Leonardo core
// pulses PB0 (RX LED) on USB reception and PB0 is the LCD chip-select on this board.
#if DEBUG_SERIAL
  #define DBG_BEGIN()  Serial.begin(115200)
  #define DBG(...)     Serial.print(__VA_ARGS__)
  #define DBGLN(...)   Serial.println(__VA_ARGS__)
#else
  #define DBG_BEGIN()  ((void)0)
  #define DBG(...)     ((void)0)
  #define DBGLN(...)   ((void)0)
#endif
