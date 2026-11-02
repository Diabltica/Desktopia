#include "tof_sensor.h"
#include "debug.h"
#include <Wire.h>

static_assert((TOF_FILTER_WINDOW % 2) == 1 && TOF_FILTER_WINDOW >= 1 && TOF_FILTER_WINDOW <= 9, "TOF_FILTER_WINDOW must be odd, 1..9");

namespace {
constexpr uint8_t kAddr7bit = (TOF_I2C_ADDR_8BIT >> 1) & 0x7F;
constexpr uint16_t kDistanceModeLong = 2;       // ST ULD: 1 = short (1.3 m), 2 = long (4 m)
constexpr uint16_t kIntermeasurementMs = TOF_TIMING_BUDGET_MS + 4;
constexpr uint32_t kBootTimeoutMs = 100;       // the chip boots in ~1.2 ms; keeps begin() well below the watchdog period
}  // namespace

TofSensor::TofSensor() : _sensor(&Wire, -1) {}   // XSHUT is pulled up on the board, not wired to the MCU

bool TofSensor::ping() {
  Wire.beginTransmission(kAddr7bit);
  return Wire.endTransmission() == 0;
}

bool TofSensor::begin() {
  _online = false;
  _everValid = false;
  _commFails = 0;
  _windowCount = 0;
  _windowIndex = 0;
  _lastInitTry = millis();

  if (!ping()) return false;
  _sensor.begin();

  uint8_t booted = 0;
  uint32_t t0 = millis();
  while (!booted) {                              // firmware boot takes a couple of ms after power-up
    if (!ping() || _sensor.VL53L1X_BootState(&booted) != 0) return false;
    if (booted) break;
    if ((uint32_t)(millis() - t0) > kBootTimeoutMs) return false;
    delay(2);
  }

  // The ST init only reports the status of its last register write, so the sensor is also
  // checked independently: it must still answer and report a plausible identifier.
  if (!ping()) return false;
  _sensor.VL53L1X_SensorInit();                   // loads the default config, runs one calibration ranging
  if (!ping()) return false;
  if (_sensor.VL53L1X_GetSensorId(&_sensorId) != 0 || _sensorId == 0x0000 || _sensorId == 0xFFFF) return false;
  DBG(F("ToF id 0x"));
  DBGLN(_sensorId, HEX);

  if (_sensor.VL53L1X_SetDistanceMode(kDistanceModeLong) != 0) return false;
  if (_sensor.VL53L1X_SetTimingBudgetInMs(TOF_TIMING_BUDGET_MS) != 0) return false;
  if (_sensor.VL53L1X_SetInterMeasurementInMs(kIntermeasurementMs) != 0) return false;
  if (_sensor.VL53L1X_StartRanging() != 0) return false;

  _nextPoll = millis();
  _online = true;
  return true;
}

bool TofSensor::hasFreshHeight(uint32_t nowMs) const {
  return _everValid && (uint32_t)(nowMs - _lastValidMs) <= TOF_STALE_TIMEOUT_MS;
}

void TofSensor::onCommFailure(uint32_t nowMs) {
  _nextPoll = nowMs + 10;
  if (++_commFails >= TOF_COMM_FAIL_LIMIT) {
    _online = false;
    _lastInitTry = nowMs;
    DBGLN(F("ToF offline"));
  }
}

void TofSensor::pushSample(uint16_t distance) {
  _window[_windowIndex] = distance;
  _windowIndex = (uint8_t)((_windowIndex + 1) % TOF_FILTER_WINDOW);
  if (_windowCount < TOF_FILTER_WINDOW) _windowCount++;

  uint16_t sorted[TOF_FILTER_WINDOW];              // median of the samples collected so far
  for (uint8_t i = 0; i < _windowCount; i++) {
    uint16_t v = _window[i];
    uint8_t j = i;
    while (j > 0 && sorted[j - 1] > v) {
      sorted[j] = sorted[j - 1];
      j--;
    }
    sorted[j] = v;
  }
  _distanceMm = sorted[_windowCount / 2];

  int32_t h = (int32_t)TOF_HEIGHT_OFFSET_MM + (int32_t)TOF_HEIGHT_SIGN * (int32_t)_distanceMm;
  if (h > 32000) h = 32000;
  if (h < -32000) h = -32000;
  _heightMm = (int16_t)h;
}

void TofSensor::update(uint32_t nowMs) {
  if (!_online) {
    if (_reinitAllowed && (uint32_t)(nowMs - _lastInitTry) >= TOF_RETRY_PERIOD_MS) begin();
    return;
  }
  if ((int32_t)(nowMs - _nextPoll) < 0) return;

  if (!ping()) {
    onCommFailure(nowMs);
    return;
  }
  uint8_t ready = 0;
  if (_sensor.VL53L1X_CheckForDataReady(&ready) != 0) {
    onCommFailure(nowMs);
    return;
  }
  if (!ready) {
    _nextPoll = nowMs + 3;
    return;
  }

  uint8_t rangeStatus = 0xFF;
  uint16_t distance = 0;
  int8_t err = _sensor.VL53L1X_GetRangeStatus(&rangeStatus);
  err |= _sensor.VL53L1X_GetDistance(&distance);
  err |= _sensor.VL53L1X_ClearInterrupt();         // also lets the next measurement start
  if (err != 0) {
    onCommFailure(nowMs);
    return;
  }

  _commFails = 0;
  _nextPoll = nowMs + (TOF_TIMING_BUDGET_MS > 10 ? TOF_TIMING_BUDGET_MS - 8 : 1);   // next sample cannot be ready sooner
  if (rangeStatus == 0 && distance > 0) {          // status 0 = valid range
    pushSample(distance);
    _lastValidMs = nowMs;
    _everValid = true;
  }
}
