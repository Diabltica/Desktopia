#include "io_expander.h"
#include <Wire.h>

namespace {
constexpr uint8_t REG_INPUT    = 0x00;
constexpr uint8_t REG_OUTPUT   = 0x01;
constexpr uint8_t REG_POLARITY = 0x02;
constexpr uint8_t REG_CONFIG   = 0x03;
}  // namespace

bool IoExpander::isPresent() {
  Wire.beginTransmission(_addr);
  return Wire.endTransmission() == 0;
}

bool IoExpander::writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool IoExpander::readReg(uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;        // repeated START
  if (Wire.requestFrom(_addr, (uint8_t)1) != 1) return false;
  value = (uint8_t)Wire.read();
  return true;
}

bool IoExpander::begin(uint8_t directionMask, uint8_t outputLevels) {
  uint8_t check = 0;
  return writeReg(REG_POLARITY, 0x00)           // no inversion: pin level is read as is
      && writeReg(REG_OUTPUT, outputLevels)
      && writeReg(REG_CONFIG, directionMask)
      && readReg(REG_CONFIG, check)
      && check == directionMask;
}

bool IoExpander::readInputs(uint8_t& value) { return readReg(REG_INPUT, value); }

bool IoExpander::writeOutputs(uint8_t value) { return writeReg(REG_OUTPUT, value); }
