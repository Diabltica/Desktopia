#pragma once
#include <stdint.h>

// Fixed texts of the user interface. Only the number travels over the cable; the HMI board owns the strings
// (hmi_firmware/src/ui_text.cpp). Append new entries at the end: the numbers are part of the link protocol.
enum class UiText : uint8_t {
  None = 0,
  Desktopia,          // DESKTOPIA
  Starting,           // starting...
  SensorBypass,       // SENSOR BYPASS
  NoSensor,           // NO SENSOR
  HeightUnknown,      // height unknown
  Limit,              // LIMIT
  MaxHeight,          // max height
  MinHeight,          // min height
  Bypass,             // BYPASS
  PresetsOff,         // presets off
  NoPreset,           // NO PRESET
  HoldToSave,         // hold to save
  NotSaved,           // not saved
  SaveError,          // SAVE ERROR
  Eeprom,             // EEPROM
  SensorLost,         // SENSOR LOST
  TofNotAnswering,    // ToF not answering
  WrongWay,           // WRONG WAY
  CheckDirSign,       // check DIR / SIGN
  Stalled,            // STALLED
  DeskNotMoving,      // desk not moving
  Timeout,            // TIMEOUT
  MoveTooLong,        // move took too long
  Watchdog,           // WATCHDOG
  LoopStuck,          // loop was stuck
  NoLink,             // NO LINK
  CheckCable,         // check the cable
  Count
};

// Everything the screen needs to know. The motherboard fills it and sends it over the cable;
// the HMI board only reads it.
struct UiModel {
  enum class Screen : uint8_t { Boot, Main, SaveGauge, SaveSuccess, Notice, Fault, Count };
  enum class Arrow  : uint8_t { None, Up, Down, Count };

  Screen  screen = Screen::Boot;
  Arrow   arrow = Arrow::None;          // direction of the current movement (Main screen)
  int16_t heightMm = 0;
  bool    heightValid = false;          // false → the number is shown as "--.-"
  uint8_t presetNumber = 1;             // 1..3 (SaveGauge / SaveSuccess)
  uint8_t gaugePercent = 0;             // 0..100 (SaveGauge)
  int16_t savedHeightMm = 0;            // SaveSuccess
  UiText  title = UiText::None;         // Boot / Notice / Fault
  UiText  subtitle = UiText::None;
};
