#pragma once
/*
 * Motherboard <-> HMI board link, shared by both firmwares (and the host simulation).
 *
 * Physical layer: RS-485 half duplex (SP485EEN on the motherboard, SP3485EN on the HMI board) over a 6P6C "RJ11/RJ12" cord,
 * 250 000 baud 8N1 – exact on the 16 MHz ATmega32U4 (UBRR = 3) and on the RP2040.
 *
 * The motherboard is the only master. Every HMI_POLL_PERIOD_MS it sends one State frame (what the screen
 * must show); the HMI board answers with one Buttons frame (debounced button levels). Nothing else is sent,
 * so the bus direction is never in doubt.
 *
 * Frame:  0xA5 | type | len | payload[len] | CRC-16/CCITT-FALSE (big endian) over type, len and payload.
 * A bad CRC drops the frame; the next poll comes 10 ms later.
 */
#include <stdint.h>
#include <stddef.h>
#include "ui_model.h"

namespace hmi {

constexpr uint32_t kBaud = 250000UL;
constexpr uint8_t kSync = 0xA5;
constexpr uint8_t kVersion = 1;
constexpr uint8_t kMaxPayload = 16;
constexpr uint8_t kMaxFrame = kMaxPayload + 5;

enum FrameType : uint8_t {
  kTypeState = 0x01,     // master → HMI: seq, packed UiModel
  kTypeButtons = 0x81,   // HMI → master: seq (echo), button bits, protocol version
};

constexpr uint8_t kUiPackedSize = 11;
constexpr uint8_t kStatePayloadSize = 1 + kUiPackedSize;
constexpr uint8_t kButtonsPayloadSize = 3;

// Button bits of the Buttons frame (1 = pressed)
constexpr uint8_t kBitUp = 0;
constexpr uint8_t kBitDown = 1;
constexpr uint8_t kBitPreset1 = 2;
constexpr uint8_t kBitPreset2 = 3;
constexpr uint8_t kBitPreset3 = 4;

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc = 0xFFFF);

// Writes a complete frame to out (kMaxFrame bytes at least); returns its length, 0 if the payload is too long.
uint8_t encodeFrame(uint8_t type, const uint8_t* payload, uint8_t len, uint8_t* out);

void packUi(const UiModel& m, uint8_t* out);                 // kUiPackedSize bytes
bool unpackUi(const uint8_t* in, UiModel& m);                // false: out-of-range field, m untouched

// Byte-by-byte receiver. feed() returns true when a frame with a valid CRC is complete; it stays readable
// until the next byte is fed.
class FrameParser {
 public:
  void reset() { _state = 0; }
  bool feed(uint8_t b);

  uint8_t type() const { return _type; }
  uint8_t length() const { return _len; }
  const uint8_t* payload() const { return _buf; }
  uint16_t crcErrors() const { return _crcErrors; }

 private:
  uint8_t _state = 0;          // 0 sync, 1 type, 2 len, 3 payload, 4 crc hi, 5 crc lo
  uint8_t _type = 0;
  uint8_t _len = 0;
  uint8_t _pos = 0;
  uint16_t _crc = 0;
  uint16_t _crcErrors = 0;
  uint8_t _buf[kMaxPayload];
};

}  // namespace hmi
