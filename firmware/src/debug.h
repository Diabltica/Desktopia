#pragma once
#include <Arduino.h>
#include "config.h"

// Optional logging on the USB CDC port (Serial). The HMI link uses Serial1, a separate UART.
#if DEBUG_SERIAL
  #define DBG_BEGIN()  Serial.begin(115200)
  #define DBG(...)     Serial.print(__VA_ARGS__)
  #define DBGLN(...)   Serial.println(__VA_ARGS__)
#else
  #define DBG_BEGIN()  ((void)0)
  #define DBG(...)     ((void)0)
  #define DBGLN(...)   ((void)0)
#endif
