#include "buttons.h"
#include "config.h"

namespace {
// TCA9554 bit of each logical button (order of enum Button).
constexpr uint8_t kBit[Buttons::kCount] = {BTN_BIT_UP, BTN_BIT_DOWN, BTN_BIT_PRESET1, BTN_BIT_PRESET2, BTN_BIT_PRESET3};
constexpr uint8_t kFirstPreset = static_cast<uint8_t>(Button::Preset1);
}  // namespace

void Buttons::reset() {
  for (uint8_t i = 0; i < kCount; i++) {
    _s[i].pressed = false;
    _s[i].consumed = false;
    _s[i].longStarted = false;
    _s[i].longDone = false;
    _s[i].diffCount = 0;
    _s[i].pressedAt = 0;
  }
  _qHead = _qTail = 0;
  _ioFails = 0;
  _ioError = false;
}

void Buttons::push(uint8_t i, ButtonEventType t) {
  uint8_t next = (uint8_t)((_qHead + 1) & 7);
  if (next == _qTail) return;                        // queue full: drop (the controller drains it every loop)
  _q[_qHead].button = static_cast<Button>(i);
  _q[_qHead].type = t;
  _qHead = next;
}

bool Buttons::nextEvent(ButtonEvent& ev) {
  if (_qHead == _qTail) return false;
  ev = _q[_qTail];
  _qTail = (uint8_t)((_qTail + 1) & 7);
  return true;
}

void Buttons::checkLongPress(uint8_t i, uint32_t nowMs) {
  State& s = _s[i];
  if (!s.pressed || s.consumed) return;
  uint32_t held = nowMs - s.pressedAt;
  if (!s.longStarted && held >= PRESET_SHORT_PRESS_MAX_MS) {
    s.longStarted = true;
    push(i, ButtonEventType::LongPressStart);
  }
  if (s.longStarted && !s.longDone && held >= PRESET_LONG_PRESS_MS) {
    s.longDone = true;
    push(i, ButtonEventType::LongPressDone);
  }
}

void Buttons::applyChange(uint8_t i, bool pressed, uint32_t nowMs) {
  State& s = _s[i];
  if (pressed) {
    s.pressed = true;
    s.pressedAt = nowMs;
    s.consumed = false;
    s.longStarted = false;
    s.longDone = false;
    push(i, ButtonEventType::Pressed);
    return;
  }
  if (i >= kFirstPreset) {
    checkLongPress(i, nowMs);                        // classify with the exact release time
    if (!s.consumed && !s.longDone) {
      push(i, s.longStarted ? ButtonEventType::LongPressAbort : ButtonEventType::ShortPress);
    }
  }
  s.pressed = false;
  s.consumed = false;
  s.longStarted = false;
  s.longDone = false;
  push(i, ButtonEventType::Released);
}

void Buttons::update(IoExpander& io, uint32_t nowMs) {
  if ((uint32_t)(nowMs - _lastSample) >= BTN_SAMPLE_PERIOD_MS) {
    _lastSample = nowMs;
    uint8_t raw = 0;
    if (!io.readInputs(raw)) {
      if (_ioFails < 255) _ioFails++;
      if (_ioFails >= BTN_IO_FAIL_LIMIT) _ioError = true;
    } else {
      _ioFails = 0;
      _ioError = false;
      for (uint8_t i = 0; i < kCount; i++) {
        bool level = ((raw >> kBit[i]) & 1) != 0;
        bool pressedNow = (BTN_ACTIVE_LEVEL == HIGH) ? level : !level;
        State& s = _s[i];
        if (pressedNow == s.pressed) {
          s.diffCount = 0;
        } else if (++s.diffCount >= BTN_DEBOUNCE_SAMPLES) {
          s.diffCount = 0;
          applyChange(i, pressedNow, nowMs);
        }
      }
    }
  }
  for (uint8_t i = kFirstPreset; i < kCount; i++) checkLongPress(i, nowMs);
}

bool Buttons::isPressed(Button b) const { return _s[static_cast<uint8_t>(b)].pressed; }

bool Buttons::isActive(Button b) const {
  const State& s = _s[static_cast<uint8_t>(b)];
  return s.pressed && !s.consumed;
}

bool Buttons::anyPressed() const {
  for (uint8_t i = 0; i < kCount; i++) {
    if (_s[i].pressed) return true;
  }
  return false;
}

uint16_t Buttons::heldMs(Button b, uint32_t nowMs) const {
  const State& s = _s[static_cast<uint8_t>(b)];
  if (!s.pressed) return 0;
  uint32_t d = nowMs - s.pressedAt;
  return d > 65535UL ? 65535 : (uint16_t)d;
}

void Buttons::consume(Button b) { _s[static_cast<uint8_t>(b)].consumed = true; }
