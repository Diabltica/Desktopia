#include "hmi_link.h"
#include "config.h"

void HmiLink::begin() {
  pinMode(_de, OUTPUT);
  digitalWrite(_de, LOW);                             // DE and /RE are tied: LOW = listen
}

void HmiLink::sendState(const UiModel& ui, uint32_t nowMs) {
  while (_port.available()) _port.read();             // nothing may be pending: only the answer to this poll counts
  _rx.reset();

  uint8_t payload[hmi::kStatePayloadSize];
  payload[0] = ++_seq;
  hmi::packUi(ui, payload + 1);
  uint8_t frame[hmi::kMaxFrame];
  uint8_t n = hmi::encodeFrame(hmi::kTypeState, payload, sizeof(payload), frame);

  digitalWrite(_de, HIGH);
  _port.write(frame, n);
  _port.flush();                                      // returns once the last stop bit is out
  digitalWrite(_de, LOW);                             // release the bus before the HMI board answers

  _waiting = true;
  _sentAt = nowMs;
  _polls++;
}

void HmiLink::update(const UiModel& ui, uint32_t nowMs) {
  if (_waiting) {
    while (_port.available()) {
      if (_rx.feed((uint8_t)_port.read()) && _rx.type() == hmi::kTypeButtons &&
          _rx.length() >= hmi::kButtonsPayloadSize && _rx.payload()[0] == _seq) {
        _buttons = _rx.payload()[1];
        _lastReply = nowMs;
        if (!_connected) _connectedSince = nowMs;
        _connected = true;
        _waiting = false;
        _replies++;
        break;
      }
    }
    if (_waiting && (uint32_t)(nowMs - _sentAt) >= HMI_REPLY_TIMEOUT_MS) _waiting = false;   // lost: next poll
  }

  if (!_waiting && (uint32_t)(nowMs - _sentAt) >= HMI_POLL_PERIOD_MS) sendState(ui, nowMs);

  if (_connected && (uint32_t)(nowMs - _lastReply) >= HMI_LINK_TIMEOUT_MS) _connected = false;
}

bool HmiLink::readInputs(uint8_t& value) const {
  if (!_connected) return false;
  value = _buttons;
  return true;
}
