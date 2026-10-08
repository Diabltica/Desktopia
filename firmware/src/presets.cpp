#include "presets.h"
#include <EEPROM.h>

namespace {
constexpr uint8_t kMagic = 0xD5;
constexpr uint8_t kVersion = 1;
}  // namespace

uint8_t Presets::crc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0xFF;                              // CRC-8, poly 0x31
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

uint8_t Presets::blobCrc(const Blob& b) { return crc8(reinterpret_cast<const uint8_t*>(&b), (uint8_t)(sizeof(Blob) - 1)); }

void Presets::begin() {
  EEPROM.get(PRESET_EEPROM_ADDR, _blob);
  if (_blob.magic == kMagic && _blob.version == kVersion && _blob.crc == blobCrc(_blob)) return;

  _blob.magic = kMagic;                            // blank or corrupted EEPROM: start with empty presets
  _blob.version = kVersion;
  for (uint8_t i = 0; i < PRESET_COUNT; i++) _blob.heights[i] = kEmpty;
  _blob.crc = blobCrc(_blob);                      // not written until the first save
}

bool Presets::isSet(uint8_t index) const { return index < PRESET_COUNT && _blob.heights[index] != kEmpty; }

uint16_t Presets::get(uint8_t index) const { return index < PRESET_COUNT ? _blob.heights[index] : kEmpty; }

bool Presets::save(uint8_t index, uint16_t heightMm) {
  if (index >= PRESET_COUNT || heightMm == kEmpty) return false;
  uint16_t previous = _blob.heights[index];
  _blob.heights[index] = heightMm;
  _blob.crc = blobCrc(_blob);
  EEPROM.put(PRESET_EEPROM_ADDR, _blob);           // put() only rewrites the bytes that changed

  Blob check;
  EEPROM.get(PRESET_EEPROM_ADDR, check);
  if (memcmp(&check, &_blob, sizeof(Blob)) == 0) return true;

  _blob.heights[index] = previous;                 // write failed: keep RAM consistent with what is stored
  _blob.crc = blobCrc(_blob);
  return false;
}
