#include "watchdog.h"
#include "config.h"
#include "motor.h"
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>

// Runs before main(): after a watchdog reset the WDT is still armed with a 15 ms timeout,
// which would boot-loop. Clear the flag and stop it, remembering why we restarted.
static uint8_t g_resetFlags __attribute__((section(".noinit")));
void earlyWatchdogOff(void) __attribute__((naked, used, section(".init3")));
void earlyWatchdogOff(void) {
  g_resetFlags = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

ISR(WDT_vect) { motorFailsafeCut(); }

namespace {
// WDE + prescaler bits: identifies "my" watchdog configuration.
constexpr uint8_t kConfigMask = _BV(WDE) | _BV(WDP3) | _BV(WDP2) | _BV(WDP1) | _BV(WDP0);
uint8_t g_myConfig = 0;
}  // namespace

namespace watchdog {

void begin() {
  wdt_enable(WATCHDOG_TIMEOUT);
  WDTCSR |= _BV(WDIE);           // interrupt first, reset on the next timeout
  g_myConfig = WDTCSR & kConfigMask;
}

void kick() {
  // The Arduino USB core resets into the bootloader (that is what `pio run -t upload` relies on) by
  // re-programming this very watchdog to 120 ms after a "1200 bps touch". Never fight it: if the
  // configuration is not mine any more, stop kicking and let it expire.
  if ((WDTCSR & kConfigMask) != g_myConfig) return;
  wdt_reset();
  WDTCSR |= _BV(WDIE);           // re-arm the interrupt stage (hardware clears it after it fires)
}

bool lastResetWasWatchdog() { return (g_resetFlags & _BV(WDRF)) != 0; }

}  // namespace watchdog
