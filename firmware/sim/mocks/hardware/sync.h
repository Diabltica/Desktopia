#pragma once
#include <stdint.h>
inline uint32_t save_and_disable_interrupts() { return 0; }   // single-threaded simulation
inline void restore_interrupts(uint32_t) {}
