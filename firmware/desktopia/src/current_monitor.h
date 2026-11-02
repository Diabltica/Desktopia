#pragma once
#include <Arduino.h>

// Motor current through the INA226 (U6, I2C 0x40) and the 2 mOhm shunt R23, on the 24 V side of the switch Q5.
//
// The INA226 compares every averaged shunt sample (8.8 ms) with MOTOR_CURRENT_LIMIT_MA by itself and pulls its
// ALERT output low (latched). That edge on PE6 / INT6 cuts the PWM and the driver supply at once, in the interrupt
// (register writes only); the loop then sees consumeAlert() and raises the fault.
class CurrentMonitor {
 public:
  bool begin();                                   // probes and configures the INA226, arms INT6
  void update(uint32_t nowMs);                    // checks the configuration, retries the init while it does not answer
  void allowReinit(bool allowed) { _reinitAllowed = allowed; }

  bool present() const { return _present; }

  // True once per alert (interrupt, or ALERT found low): the motor has been cut. Clears the INA226 latch.
  bool consumeAlert();

  bool readCurrentMa(int32_t& ma);               // signed, 1.25 mA steps; false if the INA226 does not answer

 private:
  bool configure();
  bool writeReg(uint8_t reg, uint16_t value);
  bool readReg(uint8_t reg, uint16_t& value);

  bool _present = false;
  bool _reinitAllowed = true;
  uint32_t _lastCheck = 0;
};
