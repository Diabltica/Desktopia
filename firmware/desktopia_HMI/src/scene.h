#pragma once
#include <Arduino.h>
#include "canvas.h"
#include "ui_model.h"

// The pixel-art screens: Main (at rest, manual move, automatic move), SaveGauge, SaveSuccess and Arrived.
//
// Same layout everywhere: the height in big digits with a drop shadow in the middle; around it a ring that shows the
// height as a level between ringMinMm and ringMaxMm (filled from 6 o'clock, both sides alike) with a mark for each
// saved preset; below it three pills for the preset buttons (dotted: empty, gold: the desk is on it).
//
//   at rest         lavender ring. After DISPLAY_SLEEP_AFTER_MS, "Zz" float above the height and a shine runs up
//                   the ring every 5 s.
//   Up / Down       green / orange chevrons scroll above the height and bubbles flow in the ring, both driven by
//                   the measured travel: they start with the soft start and stop with the desk.
//   automatic move  a flag with the target height, gold dots on the ring for the travel left, the target pill
//                   blinks. Arrived: a gold "ding" (the ring and the number flash, sparkles).
//   save            a floppy whose label gets written while a 24-segment ring fills (the 400 → 2000 ms gauge),
//                   then a white flash, "SAVED" with sparkles, and the preset's pill turns gold.
//
// Every frame is rebuilt from the model and the time; the animations advance every DISPLAY_ANIM_STEP_MS.
class Scene {
 public:
  void begin();                                    // ring geometry
  void reset() { _started = false; }               // another screen was shown in between: start the animations over
  void draw(Canvas& c, const UiModel& m, uint32_t nowMs);

 private:
  struct RingPx {
    uint8_t x, y;
    uint16_t cw;                                   // clockwise from 12 o'clock, tenths of a degree
  };
  enum class Pill : uint8_t { Empty, Saved, Filled, Outline };
  struct PresetLook {                              // one preset: its pill and its mark on the ring
    Pill pill;
    uint8_t pillInk;                               // Filled / Outline
    uint8_t markInk;
  };
  struct RingStyle {
    uint8_t body, head;                            // fill, and its last 4.5°
    int32_t bubbles = -1;                          // phase of the bubbles in the fill (tenths of a degree), < 0: none
    bool rising = true;                            // the bubbles flow up (or down)
    int16_t path = -1;                             // automatic move: level of the target, travel left dotted; < 0: none
    int16_t shine = -1;                            // level reached by the shine; < 0: none
  };

  void track(const UiModel& m, uint32_t nowMs);
  void trackNumber(bool valid, int16_t mm, bool moving, uint32_t nowMs);

  void drawMain(Canvas& c, const UiModel& m, uint32_t nowMs);
  void drawSaveGauge(Canvas& c, const UiModel& m, uint32_t nowMs);
  void drawSaveSuccess(Canvas& c, const UiModel& m, uint32_t nowMs);
  void drawArrival(Canvas& c, const UiModel& m, uint32_t nowMs);

  void drawLevelRing(Canvas& c, int16_t level, const RingStyle& s) const;
  void drawSegmentRing(Canvas& c, uint8_t lit, uint8_t ink, uint8_t headInk) const;
  void drawMarks(Canvas& c, const UiModel& m, const PresetLook* looks) const;
  void drawPills(Canvas& c, const PresetLook* looks) const;
  void drawNumber(Canvas& c, uint8_t ink, uint32_t nowMs) const;
  void drawChevrons(Canvas& c, bool up, int32_t rows, bool faded) const;
  void drawZz(Canvas& c, uint32_t step) const;
  void drawFlag(Canvas& c, uint16_t targetMm, uint32_t step) const;
  void drawFloppy(Canvas& c, bool white, uint8_t written) const;

  static void baseLooks(const UiModel& m, PresetLook* looks);
  int32_t progress(uint32_t nowMs) const;                  // of the current (or last) move, tenths of a chevron row

  RingPx _ring[360];                               // the ring (340 pixels)
  RingPx _tick[180];                               // just inside it: inner tick of the preset marks (164 pixels)
  uint16_t _ringCount = 0;
  uint16_t _tickCount = 0;

  bool _started = false;
  UiModel::Screen _screen = UiModel::Screen::Main;
  uint32_t _screenSince = 0;
  uint32_t _activeAt = 0;                          // last time the screen was not Main at rest

  // movement (Main with an arrow)
  bool _moving = false;
  bool _moveManual = false;                        // Up / Down (chevrons), not an automatic move (flag)
  UiModel::Arrow _moveArrow = UiModel::Arrow::None;
  uint32_t _moveSince = 0;
  uint32_t _stopSince = 0;
  bool _moveFromValid = false;
  int16_t _moveFromMm = 0;
  int16_t _travelMm = 0;                           // furthest distance from the start of the move

  // number: digit cells (0..9, 10 = dash) and the one-step roll of a digit that changed
  bool _numShown = false;
  bool _numValid = false;
  int16_t _numMm = 0;
  uint8_t _cells[3] = {0, 0, 0};
  uint8_t _cellCount = 0;
  uint8_t _tenth = 0;
  uint8_t _rollMask = 0;
  int8_t _rollDy = 0;
  uint32_t _rollUntil = 0;
};
