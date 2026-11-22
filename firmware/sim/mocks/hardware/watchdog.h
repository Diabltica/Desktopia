#pragma once
#include <stdint.h>
void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);
void watchdog_update();
bool watchdog_enable_caused_reboot();
