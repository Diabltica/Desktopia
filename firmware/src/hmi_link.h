#pragma once
#include <Arduino.h>
#include "hmi_protocol.h"
#include "ui_model.h"

// Master side of the RS-485 link to the HMI board (see shared/hmi_link/hmi_protocol.h).
// Every HMI_POLL_PERIOD_MS: sends the screen content, then listens for the button levels.
// Never blocks longer than one frame transmission (~0.7 ms at 250 kbaud).
class HmiLink {
 public:
  HmiLink(Stream& port, uint8_t dePin) : _port(port), _de(dePin) {}

  void begin();                                       // transceiver in receive mode; the port is opened by the caller
  void update(const UiModel& ui, uint32_t nowMs);     // call every loop

  // Latest button bits (hmi::kBit*). false while the link is down: the levels are not trustworthy.
  bool readInputs(uint8_t& value) const;
  bool connected() const { return _connected; }
  uint32_t connectedForMs(uint32_t nowMs) const { return _connected ? nowMs - _connectedSince : 0; }

  uint32_t polls() const { return _polls; }
  uint32_t replies() const { return _replies; }

 private:
  void sendState(const UiModel& ui, uint32_t nowMs);

  Stream& _port;
  uint8_t _de;
  hmi::FrameParser _rx;
  uint8_t _seq = 0;
  uint8_t _buttons = 0;
  bool _waiting = false;
  bool _connected = false;
  uint32_t _sentAt = 0;
  uint32_t _lastReply = 0;
  uint32_t _connectedSince = 0;
  uint32_t _polls = 0;
  uint32_t _replies = 0;
};
