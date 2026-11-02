#pragma once
#include <stdint.h>

// Fixed texts of the user interface. Only the number travels over the cable; the HMI board owns the strings
// (firmware/desktopia_HMI/src/ui_text.cpp). Append new entries at the end: the numbers are part of the link protocol.
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
  Overcurrent,        // OVERCURRENT
  MotorCurrentHigh,   // motor current high
  NoCurrentSense,     // NO CURRENT SENSE
  Ina226NotAnswering, // INA226 not answering
  Count
};

// Everything the screen needs to know. The motherboard fills it and sends it over the cable;
// the HMI board only reads it (and animates it: see firmware/desktopia_HMI/src/scene.h).
struct UiModel {
  // Arrived: an automatic move has just reached its preset (shown for OVERLAY_ARRIVED_MS, then Main).
  enum class Screen : uint8_t { Boot, Main, SaveGauge, SaveSuccess, Notice, Fault, Arrived, Count };
  enum class Arrow  : uint8_t { None, Up, Down, Count };
  enum : uint8_t { kPresetSlots = 3 };           // one per preset button
  enum : uint16_t { kNoPreset = 0xFFFF };        // presetMm[] of a preset that was never saved

  Screen  screen = Screen::Boot;
  Arrow   arrow = Arrow::None;          // direction of the current movement (Main screen)
  int16_t heightMm = 0;
  bool    heightValid = false;          // false → the number is shown as "--.-"
  uint8_t presetNumber = 0;             // 1..3: preset being saved (SaveGauge / SaveSuccess), driven to (Main, automatic
                                        // move) or just reached (Arrived). 0: none (Main: idle or manual move)
  uint8_t gaugePercent = 0;             // 0..100 (SaveGauge)
  int16_t savedHeightMm = 0;            // SaveSuccess
  UiText  title = UiText::None;         // Boot / Notice / Fault
  UiText  subtitle = UiText::None;
  uint16_t presetMm[kPresetSlots] = {kNoPreset, kNoPreset, kNoPreset};   // saved heights: ring marks, 1-2-3 pills
  int16_t ringMinMm = 0;                // the height ring is empty at ringMinMm (HEIGHT_MIN_MM)
  int16_t ringMaxMm = 0;                // and full at ringMaxMm (HEIGHT_MAX_MM)
};
