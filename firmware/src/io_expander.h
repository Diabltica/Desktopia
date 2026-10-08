#pragma once
#include <Arduino.h>

// Minimal driver for the TI TCA9554 8-bit I2C I/O expander (U2 on the Desktopia board).
class IoExpander {
 public:
  explicit IoExpander(uint8_t address7bit) : _addr(address7bit) {}

  // directionMask: 1 = input, 0 = output (CONFIG register); outputLevels: initial level of the outputs.
  // Returns true when the chip answered and the CONFIG register reads back correctly.
  bool begin(uint8_t directionMask, uint8_t outputLevels = 0x00);

  bool readInputs(uint8_t& value);     // INPUT register: the actual level of the 8 pins
  bool writeOutputs(uint8_t value);    // OUTPUT register
  bool isPresent();                    // address ACK only

 private:
  bool writeReg(uint8_t reg, uint8_t value);
  bool readReg(uint8_t reg, uint8_t& value);

  uint8_t _addr;
};
