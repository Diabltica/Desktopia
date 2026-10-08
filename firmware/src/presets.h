#pragma once
#include <Arduino.h>
#include "config.h"

// Height presets kept in the internal EEPROM (magic + version + CRC-8, written only on explicit save).
class Presets {
 public:
  enum : uint16_t { kEmpty = 0xFFFF };               // "not programmed" (an enum has no storage: no ODR-use / link issue without LTO)

  void begin();                                  // loads and validates the EEPROM image
  bool isSet(uint8_t index) const;
  uint16_t get(uint8_t index) const;             // height in mm, kEmpty when not programmed
  bool save(uint8_t index, uint16_t heightMm);   // true when the value was written and read back correctly

 private:
  struct __attribute__((packed)) Blob {
    uint8_t magic;
    uint8_t version;
    uint16_t heights[PRESET_COUNT];
    uint8_t crc;
  };

  static uint8_t crc8(const uint8_t* data, uint8_t len);
  static uint8_t blobCrc(const Blob& b);

  Blob _blob;
};
