#pragma once
#include <stdint.h>
enum clock_index { clk_sys = 5 };
inline uint32_t clock_get_hz(enum clock_index) { return 125000000u; }
