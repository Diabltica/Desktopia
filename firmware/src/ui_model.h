#pragma once
#include <Arduino.h>

// Everything the screen needs to know. The controller fills it, the display only reads it.
struct UiModel {
  enum class Screen : uint8_t { Boot, Main, SaveGauge, SaveSuccess, Notice, Fault };
  enum class Arrow  : uint8_t { None, Up, Down };

  Screen  screen = Screen::Boot;
  Arrow   arrow = Arrow::None;          // direction of the current movement (Main screen)
  int16_t heightMm = 0;
  bool    heightValid = false;          // false → the number is shown as "--.-"
  uint8_t presetNumber = 1;             // 1..PRESET_COUNT (SaveGauge / SaveSuccess)
  uint8_t gaugePercent = 0;             // 0..100 (SaveGauge)
  int16_t savedHeightMm = 0;            // SaveSuccess
  const __FlashStringHelper* title = nullptr;     // Boot / Notice / Fault
  const __FlashStringHelper* subtitle = nullptr;
};
