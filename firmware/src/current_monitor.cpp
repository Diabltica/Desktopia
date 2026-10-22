#include "current_monitor.h"
#include <Wire.h>
#include "board_pins.h"
#include "config.h"
#include "motor.h"

namespace {
// INA226 registers
constexpr uint8_t kRegConfig = 0x00;
constexpr uint8_t kRegCurrent = 0x04;
constexpr uint8_t kRegCalibration = 0x05;
constexpr uint8_t kRegMaskEnable = 0x06;
constexpr uint8_t kRegAlertLimit = 0x07;
constexpr uint8_t kRegManufacturerId = 0xFE;
constexpr uint16_t kManufacturerTi = 0x5449;   // "TI"

// 4 averages, 1.1 ms bus and shunt conversions, both continuous: a new averaged sample every 8.8 ms.
// 1.1 ms spans several PWM periods: the alert sees the mean motor current, not the PWM ripple.
constexpr uint16_t kConfig = 0x4000 | (1u << 9) | (4u << 6) | (4u << 3) | 7u;
// Current LSB 1.25 mA: calibration = 0.00512 / (current LSB x shunt)
constexpr uint32_t kCurrentLsbUa = 1250;
constexpr uint16_t kCalibration = (uint16_t)(5120000000ULL / ((uint64_t)kCurrentLsbUa * INA226_SHUNT_MICROOHM));
// Shunt over-voltage limit, 2.5 uV steps
constexpr uint32_t kAlertLimit = (uint32_t)MOTOR_CURRENT_LIMIT_MA * INA226_SHUNT_MICROOHM / 2500UL;
static_assert(kAlertLimit > 0 && kAlertLimit <= 32767, "MOTOR_CURRENT_LIMIT_MA out of the INA226 range (0 .. 40 A with 2 mOhm)");
// Mask / Enable: SOL (shunt over-voltage) alert, latched (LEN), active low (APOL = 0)
constexpr uint16_t kMaskEnable = 0x8000 | 0x0001;

volatile bool g_alert = false;

void onAlert() {                  // INT6, falling edge of the INA226 ALERT output
  motorHardCut();                 // PWM low, then Q5 open: no wait for the loop
  g_alert = true;
}
}  // namespace

bool CurrentMonitor::writeReg(uint8_t reg, uint16_t value) {
  Wire.beginTransmission((uint8_t)INA226_I2C_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(value >> 8));
  Wire.write((uint8_t)value);
  return Wire.endTransmission() == 0;
}

bool CurrentMonitor::readReg(uint8_t reg, uint16_t& value) {
  Wire.beginTransmission((uint8_t)INA226_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)INA226_I2C_ADDR, (uint8_t)2) != 2) return false;
  int hi = Wire.read();
  int lo = Wire.read();
  if (hi < 0 || lo < 0) return false;
  value = (uint16_t)(((uint16_t)hi << 8) | (uint8_t)lo);
  return true;
}

bool CurrentMonitor::configure() {
  uint16_t id = 0, me = 0;
  _present = readReg(kRegManufacturerId, id) && id == kManufacturerTi &&
             writeReg(kRegConfig, kConfig) && writeReg(kRegCalibration, kCalibration) &&
             writeReg(kRegAlertLimit, (uint16_t)kAlertLimit) && writeReg(kRegMaskEnable, kMaskEnable) &&
             readReg(kRegMaskEnable, me);                           // reading Mask / Enable clears an old latched alert
  return _present;
}

bool CurrentMonitor::begin() {
  pinMode(pins::I_ALERT, INPUT);                                     // open drain, pulled up to 5 V by R25
  configure();
  attachInterrupt(digitalPinToInterrupt(pins::I_ALERT), onAlert, FALLING);
  _lastCheck = millis();
  return _present;
}

void CurrentMonitor::update(uint32_t nowMs) {
  if ((uint32_t)(nowMs - _lastCheck) < CURRENT_MONITOR_RETRY_MS) return;
  _lastCheck = nowMs;
  if (!_present) {
    if (_reinitAllowed) configure();
    return;
  }
  // The INA226 forgets its alert settings if it ever resets: check them once per period.
  uint16_t cfg = 0, limit = 0;
  if (!readReg(kRegConfig, cfg) || !readReg(kRegAlertLimit, limit)) {
    _present = false;
  } else if (cfg != kConfig || limit != (uint16_t)kAlertLimit) {
    configure();
  }
}

bool CurrentMonitor::consumeAlert() {
  bool edge;
  noInterrupts();
  edge = g_alert;
  g_alert = false;
  interrupts();
  bool low = digitalRead(pins::I_ALERT) == LOW;                      // a low level counts too: the edge may be gone
  if (!edge && !low) return false;
  if (!edge) motorHardCut();
  uint16_t me;
  readReg(kRegMaskEnable, me);                                       // clears the latch: the next alert can fire
  return true;
}

bool CurrentMonitor::readCurrentMa(int32_t& ma) {
  uint16_t raw;
  if (!_present || !readReg(kRegCurrent, raw)) return false;
  ma = (int32_t)(int16_t)raw * (int32_t)kCurrentLsbUa / 1000;
  return true;
}
