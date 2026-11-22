/*
 * Desktopia – motorised standing desk controller, motherboard
 * RP2040 (Arduino framework: arduino-pico / PlatformIO)
 *
 * The screen and the buttons sit on the HMI board, which has its own RP2040 (firmware/desktopia_HMI/).
 * Both boards talk over RS-485 through an RJ11 cord (firmware/shared/hmi_link/hmi_protocol.h).
 *
 * One file per function:
 *   hmi_link      RS-485 master: screen content out,  buttons       debounce, short / long press
 *                 button levels in
 *   tof_sensor    VL53L1CB height through ST driver   motor         DIR + PWM, soft start / stop, driver supply
 *   current_monitor  INA226: alert on GP7 cuts the motor    presets   EEPROM (flash) storage   watchdog   failsafe
 *   desk_controller  state machine
 * All tunables: include/config.h. Pin map: include/board_pins.h.
 */
#include <Arduino.h>
#include <Wire.h>
#include <hardware/gpio.h>

#include "board_pins.h"
#include "buttons.h"
#include "config.h"
#include "current_monitor.h"
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
static CurrentMonitor current;
static DeskController controller(hmiLink, buttons, tof, motor, presets, current);

void setup() {
  motor.begin();                                   // first: PWM low, DIR defined, driver supply off – nothing may move
  DBG_BEGIN();

  hmiLink.begin();                                 // transceiver listening before the UART drives its TX pin
  Serial1.setTX(pins::RS485_TX);
  Serial1.setRX(pins::RS485_RX);
  Serial1.begin(hmi::kBaud);
  gpio_pull_up(pins::RS485_RX);                    // RO floats while this board drives the bus (DE = /RE high)

  Wire.setSDA(pins::I2C_SDA);
  Wire.setSCL(pins::I2C_SCL);
  Wire.begin();                                    // R1 / R5 (3.3k to +3V3) pull the bus up; the internal ones too
  Wire.setClock(I2C_CLOCK_HZ);
  Wire.setTimeout(I2C_TIMEOUT_MS, true);           // a stuck I2C bus can never freeze the loop

  presets.begin();
  current.begin();                                 // INA226 alert limit + GP7 interrupt, before anything can move
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
