/*
 * Desktopia – motorised standing desk controller, motherboard
 * ATmega32U4 (Arduino framework / PlatformIO)
 *
 * The screen and the buttons sit on the HMI board, which has its own RP2040 (hmi_firmware/).
 * Both boards talk over RS-485 through an RJ11 cord (shared/hmi_link/hmi_protocol.h).
 *
 * One file per function:
 *   hmi_link      RS-485 master: screen content out,  buttons       debounce, short / long press
 *                 button levels in
 *   tof_sensor    VL53L1CB height through ST driver   motor         DIR + PWM, soft start / stop
 *   presets       EEPROM storage                      watchdog      failsafe
 *   desk_controller  state machine
 * All tunables: include/config.h. Pin map: include/board_pins.h.
 */
#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"
#include "buttons.h"
#include "config.h"
#include "debug.h"
#include "desk_controller.h"
#include "hmi_link.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "watchdog.h"

static HmiLink hmiLink(Serial1, pins::RS485_DE);
static Buttons buttons;
static TofSensor tof;
static Motor motor;
static Presets presets;
static DeskController controller(hmiLink, buttons, tof, motor, presets);

void setup() {
  motor.begin();                                   // first: PWM low, DIR defined – nothing may move during the rest of the init
  DBG_BEGIN();

  hmiLink.begin();                                 // transceiver listening before the UART drives its TX pin
  Serial1.begin(hmi::kBaud);

  Wire.begin();
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setWireTimeout(I2C_TIMEOUT_US, true);       // a stuck I2C bus can never freeze the loop

  presets.begin();
  tof.begin();                                     // if it fails the controller keeps retrying while the motor is stopped
  controller.begin(millis(), watchdog::lastResetWasWatchdog());

  watchdog::begin();                               // last: nothing blocking remains after this point
  DBGLN(F("ready"));
}

void loop() {
  watchdog::kick();
  uint32_t now = millis();
  controller.update(now);
  hmiLink.update(controller.ui(), now);
}
