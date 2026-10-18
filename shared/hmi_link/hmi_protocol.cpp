#include "hmi_protocol.h"

namespace hmi {

uint16_t crc16(const uint8_t* data, size_t len, uint16_t crc) {
  while (len--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

uint8_t encodeFrame(uint8_t type, const uint8_t* payload, uint8_t len, uint8_t* out) {
  if (len > kMaxPayload) return 0;
  out[0] = kSync;
  out[1] = type;
  out[2] = len;
  for (uint8_t i = 0; i < len; i++) out[3 + i] = payload[i];
  uint16_t crc = crc16(out + 1, (size_t)len + 2);
  out[3 + len] = (uint8_t)(crc >> 8);
  out[4 + len] = (uint8_t)crc;
  return (uint8_t)(len + 5);
}

void packUi(const UiModel& m, uint8_t* out) {
  out[0] = (uint8_t)m.screen;
  out[1] = (uint8_t)m.arrow;
  out[2] = (uint8_t)m.heightMm;
  out[3] = (uint8_t)((uint16_t)m.heightMm >> 8);
  out[4] = m.heightValid ? 1 : 0;
  out[5] = m.presetNumber;
  out[6] = m.gaugePercent;
  out[7] = (uint8_t)m.savedHeightMm;
  out[8] = (uint8_t)((uint16_t)m.savedHeightMm >> 8);
  out[9] = (uint8_t)m.title;
  out[10] = (uint8_t)m.subtitle;
}

bool unpackUi(const uint8_t* in, UiModel& m) {
  if (in[0] >= (uint8_t)UiModel::Screen::Count || in[1] >= (uint8_t)UiModel::Arrow::Count || in[4] > 1 ||
      in[6] > 100 || in[9] >= (uint8_t)UiText::Count || in[10] >= (uint8_t)UiText::Count) {
    return false;
  }
  m.screen = (UiModel::Screen)in[0];
  m.arrow = (UiModel::Arrow)in[1];
  m.heightMm = (int16_t)(in[2] | ((uint16_t)in[3] << 8));
  m.heightValid = in[4] != 0;
  m.presetNumber = in[5];
  m.gaugePercent = in[6];
  m.savedHeightMm = (int16_t)(in[7] | ((uint16_t)in[8] << 8));
  m.title = (UiText)in[9];
  m.subtitle = (UiText)in[10];
  return true;
}

bool FrameParser::feed(uint8_t b) {
  switch (_state) {
    case 0:
      if (b == kSync) _state = 1;
      return false;
    case 1:
      _type = b;
      _crc = crc16(&b, 1);
      _state = 2;
      return false;
    case 2:
      if (b > kMaxPayload) {                       // cannot be a frame: hunt for the next sync byte
        _state = (b == kSync) ? 1 : 0;
        return false;
      }
      _len = b;
      _pos = 0;
      _crc = crc16(&b, 1, _crc);
      _state = _len ? 3 : 4;
      return false;
    case 3:
      _buf[_pos++] = b;
      _crc = crc16(&b, 1, _crc);
      if (_pos >= _len) _state = 4;
      return false;
    case 4:
      _state = (b == (uint8_t)(_crc >> 8)) ? 5 : 0;
      if (!_state) _crcErrors++;
      return false;
    default:
      _state = 0;
      if (b == (uint8_t)_crc) return true;
      _crcErrors++;
      return false;
  }
}

}  // namespace hmi
