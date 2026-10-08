#pragma once
#include <stdint.h>
extern volatile uint8_t WDTCSR, MCUSR;
extern unsigned g_wdrCount;
#define WDE 3
#define WDP0 0
#define WDP1 1
#define WDP2 2
#define WDP3 5
#define WDIE 6
#define WDRF 3
#define WDTO_120MS 3
#define WDTO_500MS 5
inline void wdt_reset() { g_wdrCount++; }
inline void wdt_enable(uint8_t v) { WDTCSR = (uint8_t)((1 << WDE) | ((v & 8) ? (1 << WDP3) : 0) | (v & 7)); }   // like avr-libc: WDIE is cleared
inline void wdt_disable() { WDTCSR = 0; }
