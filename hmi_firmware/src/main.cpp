/*
 * Desktopia – HMI board (round screen + 5 buttons)
 * RP2040, Arduino core arduino-pico (PlatformIO)
 *
 *   hmi_app     link slave (core 0) + screen (core 1)      display    UI screens
 *   gc9a01      panel driver                               ui_text    strings behind the UiText numbers
 * Protocol shared with the motherboard: shared/hmi_link/. Tunables: include/hmi_config.h.
 * Pin map: include/hmi_pins.h.
 */
#include <Arduino.h>
#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <pico/mutex.h>

#include "hmi_app.h"
#include "hmi_config.h"
#include "hmi_hw.h"
#include "hmi_pins.h"

auto_init_mutex(g_modelMutex);

void hmiHwLock() { mutex_enter_blocking(&g_modelMutex); }
void hmiHwUnlock() { mutex_exit(&g_modelMutex); }

void hmiHwSetLinkInverted(bool inverted) {
  gpio_set_outover(hmipins::LINK_TX, inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
  gpio_set_inover(hmipins::LINK_RX, inverted ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
}

static HmiApp app(Serial1);
static volatile uint32_t g_core1Beat = 0;

// Core 0: link to the motherboard
void setup() {
  app.beginLink(millis());
  Serial1.setTX(hmipins::LINK_TX);
  Serial1.setRX(hmipins::LINK_RX);
  Serial1.setFIFOSize(64);
  Serial1.begin(hmi::kBaud);
  watchdog_enable(HMI_WATCHDOG_MS, true);
}

void loop() {
  uint32_t now = millis();
  app.updateLink(now);
  if ((uint32_t)(now - g_core1Beat) < HMI_WATCHDOG_MS / 2) watchdog_update();   // only while core 1 lives too
}

// Core 1: screen
void setup1() {
  g_core1Beat = millis();
  app.beginDisplay();
  g_core1Beat = millis();
}

void loop1() {
  uint32_t now = millis();
  app.updateDisplay(now);
  g_core1Beat = now;
}
