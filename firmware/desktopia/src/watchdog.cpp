#include "watchdog.h"
#include "config.h"
#include "motor.h"
#include <hardware/watchdog.h>
#include <pico/time.h>

static_assert(WATCHDOG_CUT_MS >= 50 && WATCHDOG_RESET_MS > WATCHDOG_CUT_MS, "cut first, reset later");
static_assert(WATCHDOG_RESET_MS <= 8000, "RP2040 watchdog: 8.3 s at most");

namespace {
volatile uint32_t g_lastKick = 0;
repeating_timer_t g_timer;

// Timer interrupt, every 50 ms: the loop has not kicked for WATCHDOG_CUT_MS → cut the PWM and the driver supply,
// even if the loop is stuck in a blocking call. The hardware watchdog resets the chip later if it stays stuck.
bool onTick(repeating_timer_t*) {
  if ((uint32_t)(millis() - g_lastKick) >= WATCHDOG_CUT_MS) motorFailsafeCut();
  return true;
}
}  // namespace

namespace watchdog {

void begin() {
  g_lastKick = millis();
  add_repeating_timer_ms(-50, onTick, nullptr, &g_timer);
  watchdog_enable(WATCHDOG_RESET_MS, true);        // paused while a debugger halts the cores
}

void kick() {
  g_lastKick = millis();
  watchdog_update();
}

// true only after a timeout of the watchdog armed by watchdog_enable(), not after a reboot asked over USB
bool lastResetWasWatchdog() { return watchdog_enable_caused_reboot(); }

}  // namespace watchdog
