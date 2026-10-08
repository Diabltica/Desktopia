/*
 * Desktopia – motorised standing desk controller
 * ATmega32U4 (Arduino framework / PlatformIO)
 *
 * One file per function:
 *   io_expander   TCA9554 driver                     buttons         debounce, short / long press
 *   tof_sensor    VL53L1CB height through ST driver  motor           DIR + PWM, soft start / stop
 *   presets       EEPROM storage                     display         round GC9A01 screen
 *   desk_controller  state machine                   watchdog        failsafe
 * All tunables: include/config.h. Pin map: include/board_pins.h.
 */
#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"
#include "buttons.h"
#include "config.h"
#include "debug.h"
#include "desk_controller.h"
#include "display.h"
#include "io_expander.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "watchdog.h"

static IoExpander ioExpander(IOEXP_I2C_ADDR);
static Buttons buttons;
static TofSensor tof;
static Motor motor;
static Presets presets;
static Display display;
static DeskController controller(ioExpander, buttons, tof, motor, presets);

void setup() {
  motor.begin();                                   // first: PWM low, DIR defined – nothing may move during the rest of the init
  DBG_BEGIN();
  pinMode(pins::HMI_INT, INPUT_PULLUP);            // TCA9554 /INT is open-drain and the board has no pull-up (pin not used here)

  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setWireTimeout(I2C_TIMEOUT_US, true);       // a stuck I2C bus can never freeze the loop

  display.begin();
  UiModel boot;
  boot.screen = UiModel::Screen::Boot;
  boot.title = F("DESKTOPIA");
  boot.subtitle = F("starting...");
  display.render(boot, millis());

  presets.begin();
  bool ioOk = ioExpander.begin(IOEXP_DIR_MASK, 0x00);
  tof.begin();                                     // if it fails the controller keeps retrying while the motor is stopped
  controller.begin(millis(), ioOk, watchdog::lastResetWasWatchdog());

  watchdog::begin();                               // last: nothing blocking remains after this point
  DBGLN(F("ready"));
}

void loop() {
  watchdog::kick();
  uint32_t now = millis();
  controller.update(now);
  display.render(controller.ui(), now);
}
