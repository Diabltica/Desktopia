#pragma once
#include <Arduino.h>
#include <vl53l1x_class.h>      // STM32duino VL53L1X (ST ultra-lite driver port)
#include "config.h"

// Desk height from the VL53L1CB time-of-flight sensor, through ST's own driver.
//
// The ST driver retries a failed I2C read forever. Every group of driver calls is therefore
// preceded by an address ping: the driver is only entered when the sensor has just acknowledged.
class TofSensor {
 public:
  TofSensor();

  bool begin();                                   // blocking (~150-250 ms): boot wait, ST init, continuous ranging
  void update(uint32_t nowMs);                    // call every loop: polls, filters, retries init when offline
  void allowReinit(bool allowed) { _reinitAllowed = allowed; }   // keep the blocking re-init away from a running motor

  bool isOnline() const { return _online; }
  bool hasFreshHeight(uint32_t nowMs) const;      // valid sample younger than TOF_STALE_TIMEOUT_MS
  int16_t heightMm() const { return _heightMm; }  // median-filtered; meaningful only when hasFreshHeight()
  uint16_t distanceMm() const { return _distanceMm; }
  uint16_t sensorId() const { return _sensorId; }

 private:
  bool ping();
  void onCommFailure(uint32_t nowMs);
  void pushSample(uint16_t distanceMm);

  VL53L1X _sensor;
  bool _online = false;
  bool _reinitAllowed = true;
  bool _everValid = false;
  uint8_t _commFails = 0;
  uint32_t _nextPoll = 0;
  uint32_t _lastValidMs = 0;
  uint32_t _lastInitTry = 0;
  int16_t _heightMm = 0;
  uint16_t _distanceMm = 0;
  uint16_t _sensorId = 0;
  uint16_t _window[TOF_FILTER_WINDOW];
  uint8_t _windowCount = 0;
  uint8_t _windowIndex = 0;
};
