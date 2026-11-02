#pragma once
#include <Arduino.h>

// Hardware watchdog in "interrupt + reset" mode:
//  1st timeout → ISR cuts the motor PWM (even if the main loop is stuck, e.g. in a blocking driver call)
//  2nd timeout → MCU reset (PWM pin returns to high-Z, firmware restarts in a safe state)
namespace watchdog {
void begin();                    // call at the end of setup(), when nothing blocking remains
void kick();                     // call once per loop() iteration
bool lastResetWasWatchdog();
}  // namespace watchdog
