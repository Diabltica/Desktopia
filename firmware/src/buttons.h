#pragma once
#include <Arduino.h>
#include "io_expander.h"

enum class Button : uint8_t { Up = 0, Down, Preset1, Preset2, Preset3, Count };

enum class ButtonEventType : uint8_t {
  Pressed,          // debounced press (every button)
  Released,         // debounced release (every button)
  ShortPress,       // preset released before PRESET_SHORT_PRESS_MAX_MS  → go to the preset
  LongPressStart,   // preset held past PRESET_SHORT_PRESS_MAX_MS         → gauge starts
  LongPressDone,    // preset held for PRESET_LONG_PRESS_MS               → gauge full, save
  LongPressAbort    // preset released between start and done            → cancel
};

struct ButtonEvent {
  Button button;
  ButtonEventType type;
};

// Debounce + short/long press detection on top of the I/O expander inputs.
class Buttons {
 public:
  enum : uint8_t { kCount = static_cast<uint8_t>(Button::Count) };

  void reset();                                      // all buttons released, queue emptied
  void update(IoExpander& io, uint32_t nowMs);       // call every loop; samples the expander every BTN_SAMPLE_PERIOD_MS
  bool nextEvent(ButtonEvent& ev);                   // FIFO, false when empty

  bool isPressed(Button b) const;                    // debounced level
  bool isActive(Button b) const;                     // pressed and not consumed
  bool anyPressed() const;
  uint16_t heldMs(Button b, uint32_t nowMs) const;   // 0 when released, saturates at 65535

  // Ignore the rest of the current press: no short/long events, isActive() stays false until release.
  void consume(Button b);

  bool ioError() const { return _ioError; }          // expander unreachable: levels are not trustworthy

 private:
  struct State {
    bool pressed;
    bool consumed;
    bool longStarted;
    bool longDone;
    uint8_t diffCount;
    uint32_t pressedAt;
  };

  void applyChange(uint8_t i, bool pressed, uint32_t nowMs);
  void checkLongPress(uint8_t i, uint32_t nowMs);
  void push(uint8_t i, ButtonEventType t);

  State _s[kCount];
  ButtonEvent _q[8];
  uint8_t _qHead = 0;
  uint8_t _qTail = 0;
  uint32_t _lastSample = 0;
  uint8_t _ioFails = 0;
  bool _ioError = false;
};
