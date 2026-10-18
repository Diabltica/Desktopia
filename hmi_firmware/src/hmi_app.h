#pragma once
#include <Arduino.h>
#include "display.h"
#include "hmi_config.h"
#include "hmi_protocol.h"
#include "ui_model.h"

// HMI board application.
//
// Core 0 – link: answers every State frame of the motherboard with the debounced button levels, within
//   ~0.2 ms, and keeps the screen content of the last valid frame. Without valid frames it alternates the
//   UART polarity every HMI_POLARITY_SWAP_MS (A/B swapped by a reversed cord), and after HMI_NO_LINK_MS
//   it shows "NO LINK" itself.
// Core 1 – screen: draws whatever core 0 published. Drawing a full screen takes tens of milliseconds;
//   keeping it on the other core means the link never waits for it.
class HmiApp {
 public:
  explicit HmiApp(Stream& port) : _port(port) {}

  void beginLink(uint32_t nowMs);        // core 0, before the UART is opened
  void updateLink(uint32_t nowMs);       // core 0, every loop
  void beginDisplay();                   // core 1 (blocking ~0.7 s: panel reset + init)
  void updateDisplay(uint32_t nowMs);    // core 1, every loop

  // Introspection (tests, debug)
  bool linkUp(uint32_t nowMs) const { return _everLinked && (uint32_t)(nowMs - _lastFrame) < HMI_NO_LINK_MS; }
  bool inverted() const { return _inverted; }
  uint8_t buttons() const { return _stable; }
  uint32_t framesOk() const { return _framesOk; }

 private:
  void sampleButtons(uint32_t nowMs);
  void answer(uint8_t seq);
  void publish(const UiModel& m);

  Stream& _port;
  hmi::FrameParser _rx;

  // link (core 0)
  UiModel _received;
  bool _everLinked = false;
  bool _inverted = false;
  uint32_t _lastFrame = 0;
  uint32_t _lastSwap = 0;
  uint32_t _framesOk = 0;

  // buttons (core 0)
  uint8_t _stable = 0;                   // debounced, 1 = pressed, bits hmi::kBit*
  uint8_t _pending = 0;                  // bits whose raw level differs from _stable
  uint32_t _since[5] = {0, 0, 0, 0, 0};

  // hand-over core 0 → core 1 (under hmiHwLock)
  UiModel _shared;

  // screen (core 1)
  Display _display;
};
