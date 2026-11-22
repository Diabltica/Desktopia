#pragma once
#include <Arduino.h>

// Two-stage watchdog:
//  WATCHDOG_CUT_MS without a kick   → a timer interrupt cuts the motor PWM and the driver supply (even if the main
//                                     loop is stuck, e.g. in a blocking driver call)
//  WATCHDOG_RESET_MS without a kick → the RP2040 hardware watchdog resets the chip (every pin returns to an input with
//                                     its pull-down, the firmware restarts in a safe state)
namespace watchdog {
void begin();                    // call at the end of setup(), when nothing blocking remains
void kick();                     // call once per loop() iteration
bool lastResetWasWatchdog();
}  // namespace watchdog
