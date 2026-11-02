#include "scene.h"
#include <math.h>
#include "hmi_config.h"

namespace {

using Screen = UiModel::Screen;
using Arrow = UiModel::Arrow;

// --- Layout, in logical pixels (Canvas: 60 x 60, 4 x 4 screen pixels each)
constexpr int16_t kDigitY = 23;          // big digits 10 x 14, one every 12 pixels, the block centred
constexpr int16_t kTenthY = 30;          // small digit 5 x 7, bottom-aligned with the big ones
constexpr int16_t kPointY = 35;          // decimal point 2 x 2
constexpr int16_t kUnitY = 24;           // "cm" above the small digit
constexpr int16_t kPillX = 17;           // pills 7 x 9, one every 10 pixels
constexpr int16_t kPillY = 41;
constexpr int16_t kChevronX = 24;        // chevrons 12 x 6, one every 7 rows, scrolling through rows 8..20
constexpr int16_t kChevronTop = 8;
constexpr int16_t kChevronBottom = 20;
constexpr int16_t kZzX = 23;             // bottom-left corner of a newborn "z"
constexpr int16_t kZzY = 19;
constexpr int16_t kFlagY = 11;           // pole 9 high, cloth 6 x 4 waving by one row, target height on its right
constexpr int16_t kFloppyX = 24;         // 12 x 12
constexpr int16_t kFloppyY = 8;

// --- Ring. Logical pixel (x, y): dx = 2x + 1 - 60, dy = 2y + 1 - 60 (doubled distances to the centre).
constexpr int32_t kRingIn2 = 2766;       // the ring: dx² + dy² in 2766 .. 3206, radius 26.3 .. 28.3
constexpr int32_t kRingOut2 = 3206;
constexpr int32_t kTickIn2 = 2382;       // inner tick of a preset mark: radius 24.4 .. 25.5
constexpr int32_t kTickOut2 = 2600;
constexpr float kPi = 3.14159265f;
// Angles in tenths of a degree. Level = angle from 6 o'clock, the same on both sides: 0 (bottom) .. 1800 (top).
constexpr int16_t kHead = 45;            // the last 4.5° of a fill are lighter
constexpr int16_t kMarkHalf = 21;        // a preset mark covers ±2.1°
constexpr int16_t kBubbleGap = 360;      // one bubble every 36°, 4° long
constexpr int16_t kBubbleLen = 40;
constexpr int16_t kShineHalf = 22;       // the shine is 4.4° wide and climbs 3.5° per step
constexpr int16_t kShineSpeed = 35;
constexpr uint8_t kSegments = 24;        // save gauge: 10° dashes, one every 15°, the first one at 2.5°
constexpr int16_t kSegmentPitch = 150;
constexpr int16_t kSegmentLen = 100;
constexpr int16_t kSegmentStart = 25;

// --- Timing, in animation steps (DISPLAY_ANIM_STEP_MS)
constexpr uint32_t kFadeSteps = 2;       // chevrons dark for 2 steps when a move starts and when it ends
constexpr uint32_t kZzEvery = 16;        // a new "z" every 16 steps; each one lives 27 steps
constexpr int32_t kZzLife = 27;
constexpr uint32_t kShineEvery = 64;     // the shine runs once every 64 steps, from step 8 of the cycle
constexpr uint32_t kShineStart = 8;
constexpr uint32_t kBlinkSteps = 3;      // a blinking pill: 3 steps filled, 3 steps outlined
constexpr uint32_t kFlagSteps = 9;       // the flag stays this long on the arrival screen

// --- Sprites: one uint16_t per row, the leftmost column in the highest bit used
const uint16_t kBigDigits[11][14] = {     // 10 x 14; 10 = dash ("--.-": no height)
  {0x0FC, 0x1FE, 0x387, 0x303, 0x303, 0x303, 0x303, 0x303, 0x303, 0x303, 0x303, 0x387, 0x1FE, 0x0FC},
  {0x030, 0x070, 0x0F0, 0x0F0, 0x070, 0x030, 0x030, 0x030, 0x030, 0x030, 0x030, 0x078, 0x0FC, 0x0FC},
  {0x0FC, 0x1FE, 0x387, 0x303, 0x003, 0x007, 0x00E, 0x01C, 0x038, 0x070, 0x0C0, 0x1C0, 0x3FF, 0x3FF},
  {0x0FC, 0x1FE, 0x387, 0x303, 0x003, 0x007, 0x03C, 0x03C, 0x007, 0x003, 0x303, 0x387, 0x1FE, 0x0FC},
  {0x00C, 0x01C, 0x03C, 0x07C, 0x0CC, 0x1CC, 0x30C, 0x31E, 0x3FF, 0x1FF, 0x01E, 0x00C, 0x00C, 0x00C},
  {0x1FF, 0x3FF, 0x300, 0x300, 0x3FC, 0x1FE, 0x007, 0x003, 0x003, 0x003, 0x303, 0x387, 0x1FE, 0x0FC},
  {0x0FC, 0x1FC, 0x380, 0x300, 0x300, 0x380, 0x3FC, 0x3FE, 0x387, 0x303, 0x303, 0x387, 0x1FE, 0x0FC},
  {0x3FE, 0x3FF, 0x003, 0x003, 0x00E, 0x01C, 0x038, 0x070, 0x0E0, 0x0C0, 0x0C0, 0x0C0, 0x0C0, 0x0C0},
  {0x0FC, 0x1FE, 0x387, 0x303, 0x303, 0x387, 0x0FC, 0x0FC, 0x387, 0x303, 0x303, 0x387, 0x1FE, 0x0FC},
  {0x0FC, 0x1FE, 0x387, 0x303, 0x303, 0x387, 0x1FF, 0x0FF, 0x007, 0x003, 0x003, 0x007, 0x0FE, 0x0FC},
  {0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x0FC, 0x0FC, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000},
};
const uint16_t kSmallDigits[11][7] = {    // 5 x 7; 10 = dash
  {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
  {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
  {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
  {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
  {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
  {0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00},
};
const uint16_t kTiny[15][5] = {           // 3 x 5: digits 0-9, then S A V E D
  {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 3, 1, 7}, {5, 5, 7, 1, 1},
  {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
  {3, 4, 2, 1, 6}, {2, 5, 7, 5, 5}, {5, 5, 5, 5, 2}, {7, 4, 6, 4, 7}, {6, 5, 5, 5, 6},
};
const uint8_t kSaved[5] = {10, 11, 12, 13, 14};
const uint16_t kUnit[4] = {0x0DA, 0x115, 0x115, 0x0D5};                   // "cm", 9 x 4
const uint16_t kChevron[6] = {0x060, 0x0F0, 0x1F8, 0x39C, 0x70E, 0xE07};  // 12 x 6, pointing up
const uint16_t kZSmall[4] = {0xF, 0x2, 0x4, 0xF};
const uint16_t kZBig[5] = {0x1F, 0x02, 0x04, 0x08, 0x1F};
const uint16_t kCheck[5] = {0x03, 0x06, 0xCC, 0x78, 0x30};                 // 8 x 5
const uint16_t kFloppy[12] = {0xFFC, 0xFFE, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF, 0xFFF};
const uint16_t kShutter[5] = {0x000, 0x1C8, 0x1C8, 0x1C8, 0x1F8};
const uint16_t kShutterHole[4] = {0x000, 0x030, 0x030, 0x030};
// Sparkles of the SAVED screen: x, y, step of birth
const int8_t kSavedSparkles[5][3] = {{15, 13, 2}, {47, 41, 3}, {45, 13, 4}, {12, 41, 6}, {30, 4, 8}};

uint32_t steps(uint32_t nowMs, uint32_t sinceMs) { return (uint32_t)(nowMs - sinceMs) / DISPLAY_ANIM_STEP_MS; }

int32_t dist(int32_t a, int32_t b) { return a > b ? a - b : b - a; }

int16_t levelOfPx(uint16_t cw) { return (int16_t)(cw <= 1800 ? 1800 - cw : cw - 1800); }

// Height in mm → ring level
int16_t levelOf(const UiModel& m, int32_t mm) {
  const int32_t span = (int32_t)m.ringMaxMm - m.ringMinMm;
  if (span <= 0) return 0;
  const int32_t v = (mm - m.ringMinMm) * 1800 / span;
  return (int16_t)(v < 0 ? 0 : (v > 1800 ? 1800 : v));
}

int8_t presetIndex(const UiModel& m) {
  return (m.presetNumber >= 1 && m.presetNumber <= UiModel::kPresetSlots) ? (int8_t)(m.presetNumber - 1) : -1;
}

bool saved(const UiModel& m, int8_t i) { return i >= 0 && m.presetMm[i] != UiModel::kNoPreset; }

// At rest on a preset: its pill and its mark turn gold
bool onPreset(const UiModel& m, uint8_t i) {
  return m.heightValid && saved(m, (int8_t)i) && dist(m.heightMm, m.presetMm[i]) <= DISPLAY_PRESET_MATCH_MM;
}

bool blinkOn(uint32_t step) { return (step / kBlinkSteps) % 2 == 0; }

bool bubbleAt(int16_t level, int32_t phase, bool rising) {
  int32_t k = (rising ? level - phase : level + phase) % kBubbleGap;
  if (k < 0) k += kBubbleGap;
  return k < kBubbleLen;
}

void tiny(Canvas& c, int16_t x, int16_t y, uint8_t glyph, uint8_t ink) { c.sprite(x, y, kTiny[glyph], 3, 5, ink); }

// Born at age 0: white dot, small cross, big cross (2 steps), diagonal cross, gold dot, gone.
void sparkle(Canvas& c, int16_t x, int16_t y, int32_t age) {
  if (age < 0 || age > 5) return;
  if (age == 5) {
    c.set(x, y, kGold);
    return;
  }
  if (age == 4) {
    for (int16_t d = -1; d <= 1; d += 2) {
      c.set((int16_t)(x - 1), (int16_t)(y + d), kGold);
      c.set((int16_t)(x + 1), (int16_t)(y + d), kGold);
    }
  } else if (age >= 1) {
    for (int16_t k = 1; k <= (age == 1 ? 1 : 2); k++) {
      c.set((int16_t)(x - k), y, kGold);
      c.set((int16_t)(x + k), y, kGold);
      c.set(x, (int16_t)(y - k), kGold);
      c.set(x, (int16_t)(y + k), kGold);
    }
  }
  c.set(x, y, kWhite);
}

// Point of the ring at a preset's level, left side (the right one is its mirror)
void markPoint(const UiModel& m, uint8_t preset, int16_t& x, int16_t& y) {
  const float a = (float)(1800 - levelOf(m, m.presetMm[preset])) * (kPi / 1800.0f);   // from 12 o'clock
  x = (int16_t)floorf(30.0f - 27.3f * sinf(a));
  y = (int16_t)floorf(30.0f - 27.3f * cosf(a));
}

}  // namespace

void Scene::begin() {
  _ringCount = _tickCount = 0;
  for (int16_t y = 0; y < Canvas::kSize; y++) {
    for (int16_t x = 0; x < Canvas::kSize; x++) {
      const int32_t dx = 2 * x + 1 - Canvas::kSize, dy = 2 * y + 1 - Canvas::kSize;
      const int32_t d2 = dx * dx + dy * dy;
      const bool ring = d2 >= kRingIn2 && d2 <= kRingOut2;
      const bool tick = d2 >= kTickIn2 && d2 <= kTickOut2;
      if (!ring && !tick) continue;
      float deg = atan2f((float)dx, (float)-dy) * (180.0f / kPi);
      if (deg < 0) deg += 360.0f;
      RingPx p = {(uint8_t)x, (uint8_t)y, (uint16_t)((uint16_t)(deg * 10.0f + 0.5f) % 3600)};
      if (ring && _ringCount < sizeof(_ring) / sizeof(_ring[0])) _ring[_ringCount++] = p;
      if (tick && _tickCount < sizeof(_tick) / sizeof(_tick[0])) _tick[_tickCount++] = p;
    }
  }
}

void Scene::draw(Canvas& c, const UiModel& m, uint32_t nowMs) {
  track(m, nowMs);
  c.clear(kNavy);
  switch (m.screen) {
    case Screen::SaveGauge:   drawSaveGauge(c, m, nowMs); break;
    case Screen::SaveSuccess: drawSaveSuccess(c, m, nowMs); break;
    case Screen::Arrived:     drawArrival(c, m, nowMs); break;
    default:                  drawMain(c, m, nowMs); break;
  }
}

// ---------------------------------------------------------------------------------------------
//  What changed since the last frame
// ---------------------------------------------------------------------------------------------
void Scene::track(const UiModel& m, uint32_t nowMs) {
  if (!_started || m.screen != _screen) {
    _screen = m.screen;
    _screenSince = nowMs;
  }
  if (!_started) {
    _started = true;
    _activeAt = nowMs;
    _moving = false;
    _moveManual = false;
    _numShown = false;
  }

  const bool moving = m.screen == Screen::Main && m.arrow != Arrow::None;
  if (moving && (!_moving || m.arrow != _moveArrow)) {     // a move starts (or turns round between two frames)
    _moveArrow = m.arrow;
    _moveManual = presetIndex(m) < 0;
    _moveSince = nowMs;
    _moveFromValid = m.heightValid;
    _moveFromMm = m.heightMm;
    _travelMm = 0;
  }
  if (moving && m.heightValid) {
    if (!_moveFromValid) {
      _moveFromValid = true;
      _moveFromMm = m.heightMm;
    }
    const int16_t d = (int16_t)dist(m.heightMm, _moveFromMm);
    if (d > _travelMm) _travelMm = d;                      // never backwards: sensor noise cannot rewind the chevrons
  }
  if (_moving && !moving) _stopSince = nowMs;
  _moving = moving;
  if (m.screen != Screen::Main || moving) _activeAt = nowMs;

  if (m.screen == Screen::SaveSuccess) trackNumber(true, m.savedHeightMm, false, nowMs);
  else trackNumber(m.heightValid, m.heightMm, moving, nowMs);
}

// The number follows the height (every millimetre while moving, past DISPLAY_DEADBAND_MM at rest). While moving, a
// big digit that changes rolls in: drawn 2 pixels low (rising) or high (falling) for one step. At rest the changes
// are sensor noise: no roll.
void Scene::trackNumber(bool valid, int16_t mm, bool moving, uint32_t nowMs) {
  if (_numShown && valid == _numValid && (!valid || dist(mm, _numMm) < (moving ? 1 : DISPLAY_DEADBAND_MM))) return;

  uint8_t cells[3] = {10, 10, 10};
  uint8_t count = 2;
  uint8_t tenth = 10;
  if (valid) {
    const int16_t h = mm < 0 ? 0 : (mm > 9999 ? 9999 : mm);
    uint16_t cm = (uint16_t)(h / 10);
    tenth = (uint8_t)(h % 10);
    count = cm >= 100 ? 3 : (cm >= 10 ? 2 : 1);
    for (int8_t i = (int8_t)(count - 1); i >= 0; i--) {
      cells[i] = (uint8_t)(cm % 10);
      cm /= 10;
    }
  }
  if (moving && _numShown && _numValid && valid) {
    uint8_t mask = 0;
    for (uint8_t i = 0; i < count; i++) {
      if (count != _cellCount || cells[i] != _cells[i]) mask |= (uint8_t)(1u << i);
    }
    if (mask) {
      _rollMask = mask;
      _rollDy = mm > _numMm ? 2 : -2;
      _rollUntil = nowMs + DISPLAY_ANIM_STEP_MS;
    }
  }
  memcpy(_cells, cells, sizeof(_cells));
  _cellCount = count;
  _tenth = tenth;
  _numMm = mm;
  _numValid = valid;
  _numShown = true;
}

// Progress of the current (or last) move in tenths of a chevron row: one row per 2.5 mm of measured travel, or one
// row per step when there is no height (sensor bypass).
int32_t Scene::progress(uint32_t nowMs) const {
  if (_moveFromValid) return (int32_t)_travelMm * 4;
  return (int32_t)steps(_moving ? nowMs : _stopSince, _moveSince) * 10;
}

// ---------------------------------------------------------------------------------------------
//  Screens
// ---------------------------------------------------------------------------------------------
void Scene::drawMain(Canvas& c, const UiModel& m, uint32_t nowMs) {
  const bool moving = m.arrow != Arrow::None;
  const bool up = m.arrow == Arrow::Up;
  const int8_t target = moving && saved(m, presetIndex(m)) ? presetIndex(m) : -1;   // automatic move
  const uint32_t moveStep = steps(nowMs, _moveSince);
  const bool sleepy = !moving && (uint32_t)(nowMs - _activeAt) >= DISPLAY_SLEEP_AFTER_MS;
  const uint32_t sleepStep = steps(nowMs, _activeAt + DISPLAY_SLEEP_AFTER_MS);

  PresetLook looks[UiModel::kPresetSlots];
  baseLooks(m, looks);
  RingStyle ring;
  if (moving) {
    ring.body = up ? kGreen : kOrange;
    ring.head = up ? kMint : kPeach;
    ring.bubbles = progress(nowMs) * 18 / 5;               // 3.6° per chevron row
    ring.rising = up;
    if (target >= 0) {
      ring.path = levelOf(m, m.presetMm[target]);
      looks[target] = {blinkOn(moveStep) ? Pill::Filled : Pill::Outline, kGold, kPaleGold};
    }
  } else {
    ring.body = kLavender;
    ring.head = kLilac;
    const uint32_t cycle = sleepStep % kShineEvery;
    if (sleepy && cycle >= kShineStart) ring.shine = (int16_t)((cycle - kShineStart) * kShineSpeed);
    for (uint8_t i = 0; i < UiModel::kPresetSlots; i++) {
      if (onPreset(m, i)) looks[i] = {Pill::Filled, kGold, kGold};
    }
  }
  drawLevelRing(c, m.heightValid ? levelOf(m, m.heightMm) : -1, ring);
  drawMarks(c, m, looks);

  if (target >= 0) drawFlag(c, m.presetMm[target], moveStep);
  else if (moving) drawChevrons(c, up, progress(nowMs) / 10, moveStep < kFadeSteps);
  else if (_moveManual && steps(nowMs, _stopSince) < kFadeSteps) drawChevrons(c, _moveArrow == Arrow::Up, progress(nowMs) / 10, true);
  else if (sleepy) drawZz(c, sleepStep);

  drawNumber(c, kCream, nowMs);
  drawPills(c, looks);
}

// Long press: the gauge (gaugePercent, 400 → 2000 ms of hold) fills the ring clockwise from 12 o'clock while the
// floppy's label gets written.
void Scene::drawSaveGauge(Canvas& c, const UiModel& m, uint32_t nowMs) {
  const uint32_t step = steps(nowMs, _screenSince);
  const uint8_t pct = m.gaugePercent;
  drawSegmentRing(c, (uint8_t)((pct * kSegments + 99) / 100), kCyan, pct < 100 ? kIce : kCyan);
  drawFloppy(c, step == 0, (uint8_t)((pct * 12 + 50) / 100));
  drawNumber(c, kCream, nowMs);

  PresetLook looks[UiModel::kPresetSlots];
  baseLooks(m, looks);
  const int8_t slot = presetIndex(m);
  if (slot >= 0) looks[slot] = {blinkOn(step) ? Pill::Filled : Pill::Outline, kCyan, kCyan};
  drawPills(c, looks);
}

// Saved: white flash (2 steps), then a gold ring, a check mark, "SAVED" and sparkles.
void Scene::drawSaveSuccess(Canvas& c, const UiModel& m, uint32_t nowMs) {
  const uint32_t step = steps(nowMs, _screenSince);
  const bool flash = step < 2;
  const uint8_t ink = flash ? kWhite : kGold;
  drawSegmentRing(c, kSegments, ink, ink);
  if (step == 0) drawFloppy(c, true, 0);
  if (!flash) {
    c.sprite(26, 8, kCheck, 8, 5, kGold);
    for (uint8_t i = 0; i < 5; i++) tiny(c, (int16_t)(20 + 4 * i), 15, kSaved[i], kGold);
  }
  drawNumber(c, flash ? kWhite : (step < 6 ? kGold : kCream), nowMs);

  PresetLook looks[UiModel::kPresetSlots];
  baseLooks(m, looks);
  const int8_t slot = presetIndex(m);
  if (slot >= 0) looks[slot] = {Pill::Filled, ink, ink};
  drawPills(c, looks);
  for (const int8_t* s : kSavedSparkles) sparkle(c, s[0], s[1], (int32_t)step - s[2]);
}

// Preset reached: the ring flashes gold twice, the number is gold for a moment, sparkles on the preset's marks
// and then by its pill. The flag goes away after kFlagSteps.
void Scene::drawArrival(Canvas& c, const UiModel& m, uint32_t nowMs) {
  const uint32_t step = steps(nowMs, _screenSince);
  const int8_t target = saved(m, presetIndex(m)) ? presetIndex(m) : -1;
  const bool gold = step < 2 || (step >= 4 && step < 6);

  RingStyle ring;
  ring.body = gold ? kGold : kLavender;
  ring.head = gold ? kPaleGold : kLilac;
  drawLevelRing(c, m.heightValid ? levelOf(m, m.heightMm) : -1, ring);
  PresetLook looks[UiModel::kPresetSlots];
  baseLooks(m, looks);
  if (target >= 0) looks[target] = {Pill::Filled, kGold, kGold};
  drawMarks(c, m, looks);
  if (target >= 0 && step < kFlagSteps) drawFlag(c, m.presetMm[target], steps(nowMs, _moveSince));
  drawNumber(c, step < 4 ? kGold : kCream, nowMs);
  drawPills(c, looks);

  if (target >= 0) {
    int16_t x, y;
    markPoint(m, (uint8_t)target, x, y);
    sparkle(c, x, y, (int32_t)step);
    sparkle(c, (int16_t)(Canvas::kSize - 1 - x), y, (int32_t)step);
    sparkle(c, (int16_t)(kPillX + 10 * target + 9), (int16_t)(kPillY - 2), (int32_t)step - 4);
  }
}

// ---------------------------------------------------------------------------------------------
//  Parts
// ---------------------------------------------------------------------------------------------
void Scene::baseLooks(const UiModel& m, PresetLook* looks) {
  for (uint8_t i = 0; i < UiModel::kPresetSlots; i++) {
    looks[i] = {saved(m, (int8_t)i) ? Pill::Saved : Pill::Empty, kSteel, kSteel};
  }
}

// Height ring: filled from 6 o'clock up to `level` on both sides, the last 4.5° lighter.
void Scene::drawLevelRing(Canvas& c, int16_t level, const RingStyle& s) const {
  for (uint16_t k = 0; k < _ringCount; k++) {
    const RingPx& p = _ring[k];
    const int16_t u = levelOfPx(p.cw);
    const bool dot = ((p.x + p.y) & 1) == 0;               // the travel left is dotted (checkerboard)
    uint8_t ink = kTrack;
    if (u <= level) {
      if (u > level - kHead) ink = s.head;
      else if (s.path >= 0 && u > s.path) ink = dot ? kOchre : kTrack;           // going down: target .. here
      else if (s.shine >= 0 && dist(u, s.shine) <= kShineHalf) ink = kWhite;
      else if (s.bubbles >= 0 && bubbleAt(u, s.bubbles, s.rising)) ink = s.head;
      else ink = s.body;
    } else if (u <= s.path) {
      ink = dot ? kOchre : kTrack;                                                // going up: here .. target
    }
    c.set(p.x, p.y, ink);
  }
}

// Save gauge: kSegments dashes clockwise from 12 o'clock, the first `lit` ones on, the newest in headInk.
void Scene::drawSegmentRing(Canvas& c, uint8_t lit, uint8_t ink, uint8_t headInk) const {
  for (uint16_t k = 0; k < _ringCount; k++) {
    const RingPx& p = _ring[k];
    const int16_t a = (int16_t)((p.cw + 3600 - kSegmentStart) % 3600);
    if (a % kSegmentPitch >= kSegmentLen) continue;        // gap between two dashes
    const uint8_t seg = (uint8_t)(a / kSegmentPitch);
    c.set(p.x, p.y, seg >= lit ? (uint8_t)kTrack : (seg + 1 == lit ? headInk : ink));
  }
}

// A mark per saved preset, on both sides: the ring over ±2.1° and a tick just inside it.
void Scene::drawMarks(Canvas& c, const UiModel& m, const PresetLook* looks) const {
  for (uint8_t i = 0; i < UiModel::kPresetSlots; i++) {
    if (!saved(m, (int8_t)i)) continue;
    const int16_t at = levelOf(m, m.presetMm[i]);
    for (uint16_t k = 0; k < _ringCount; k++) {
      if (dist(levelOfPx(_ring[k].cw), at) <= kMarkHalf) c.set(_ring[k].x, _ring[k].y, looks[i].markInk);
    }
    for (uint16_t k = 0; k < _tickCount; k++) {
      if (dist(levelOfPx(_tick[k].cw), at) <= kMarkHalf) c.set(_tick[k].x, _tick[k].y, looks[i].markInk);
    }
  }
}

void Scene::drawPills(Canvas& c, const PresetLook* looks) const {
  for (uint8_t i = 0; i < UiModel::kPresetSlots; i++) {
    const PresetLook& l = looks[i];
    const int16_t x = (int16_t)(kPillX + 10 * i);
    if (l.pill == Pill::Filled) {
      c.fill((int16_t)(x + 1), kPillY, 5, 9, l.pillInk);
      c.fill(x, (int16_t)(kPillY + 1), 7, 7, l.pillInk);
      tiny(c, (int16_t)(x + 2), (int16_t)(kPillY + 2), (uint8_t)(1 + i), kNavy);
      continue;
    }
    const uint8_t edge = l.pill == Pill::Empty ? (uint8_t)kDusk : (l.pill == Pill::Saved ? (uint8_t)kSteel : l.pillInk);
    for (int16_t dy = 0; dy < 9; dy++) {
      for (int16_t dx = 0; dx < 7; dx++) {
        if ((dx == 0 || dx == 6) == (dy == 0 || dy == 8)) continue;        // inside, or a rounded corner
        if (l.pill == Pill::Empty && ((dx + dy) & 1)) continue;             // never saved: dotted
        c.set((int16_t)(x + dx), (int16_t)(kPillY + dy), edge);
      }
    }
    tiny(c, (int16_t)(x + 2), (int16_t)(kPillY + 2), (uint8_t)(1 + i), l.pill == Pill::Saved ? (uint8_t)kMist : edge);
  }
}

// Big integer digits, point, small tenth, "cm": the block is centred, each glyph has a drop shadow.
void Scene::drawNumber(Canvas& c, uint8_t ink, uint32_t nowMs) const {
  if (!_numValid) ink = kMist;
  const bool rolling = (int32_t)(nowMs - _rollUntil) < 0;
  const int16_t x0 = (int16_t)(26 - 6 * _cellCount);
  for (uint8_t i = 0; i < _cellCount; i++) {
    const int16_t x = (int16_t)(x0 + 12 * i);
    const int16_t y = (int16_t)(kDigitY + (rolling && (_rollMask & (1u << i)) ? _rollDy : 0));
    c.sprite((int16_t)(x + 1), (int16_t)(y + 1), kBigDigits[_cells[i]], 10, 14, kShadow);
    c.sprite(x, y, kBigDigits[_cells[i]], 10, 14, ink);
  }
  const int16_t xp = (int16_t)(x0 + 12 * _cellCount);
  c.fill((int16_t)(xp + 1), (int16_t)(kPointY + 1), 2, 2, kShadow);
  c.fill(xp, kPointY, 2, 2, ink);
  c.sprite((int16_t)(xp + 4), (int16_t)(kTenthY + 1), kSmallDigits[_tenth], 5, 7, kShadow);
  c.sprite((int16_t)(xp + 3), kTenthY, kSmallDigits[_tenth], 5, 7, ink);
  c.sprite(xp, kUnitY, kUnit, 9, 4, kMist);
}

// Chevrons scrolling `rows` rows in the direction of the move, lit by a fixed gradient: brightest near the leading
// edge, dark at both ends of the band. `faded`: all dark (first and last steps of a move).
void Scene::drawChevrons(Canvas& c, bool up, int32_t rows, bool faded) const {
  const uint8_t dark = up ? kForest : kRust, base = up ? kGreen : kOrange, light = up ? kMint : kPeach;
  for (int16_t y = kChevronTop; y <= kChevronBottom; y++) {
    const int32_t k = (((up ? rows : -rows) + (y - kChevronTop)) % 7 + 7) % 7;
    if (k > 5) continue;                                   // gap between two chevrons
    const int16_t lead = up ? (int16_t)(y - kChevronTop) : (int16_t)(kChevronBottom - y);
    const uint8_t ink = (faded || lead < 2 || lead > 9) ? dark : (lead < 6 ? light : base);
    c.bits(kChevronX, y, kChevron[up ? k : 5 - k], 12, ink);
  }
}

// A "z" is born every kZzEvery steps, small and dark; it rises, drifts right, brightens, grows into a "Z", fades out.
void Scene::drawZz(Canvas& c, uint32_t step) const {
  for (uint32_t k = 0; k < 2 && step >= k * kZzEvery; k++) {
    const int32_t age = (int32_t)(step % kZzEvery + k * kZzEvery);
    if (age >= kZzLife) continue;
    const uint8_t ink = age < 3 ? kPlum : (age < 8 ? kLilac : (age < 22 ? kLavender : kPlum));
    const int16_t x = (int16_t)(kZzX + age / 4), bottom = (int16_t)(kZzY - age / 3);
    if (age < 13) c.sprite(x, (int16_t)(bottom - 3), kZSmall, 4, 4, ink);
    else c.sprite(x, (int16_t)(bottom - 4), kZBig, 5, 5, ink);
  }
}

// Flag (pole + cloth waving by one row) and the target height in small digits, centred together.
void Scene::drawFlag(Canvas& c, uint16_t targetMm, uint32_t step) const {
  const uint16_t mm = targetMm > 9999 ? 9999 : targetMm;
  uint16_t cm = (uint16_t)(mm / 10);
  const uint8_t count = cm >= 100 ? 3 : (cm >= 10 ? 2 : 1);
  int16_t x = (int16_t)((Canvas::kSize - (9 + 4 * count + 5)) / 2);
  c.fill(x, kFlagY, 1, 9, kMist);
  for (uint8_t col = 0; col < 6; col++) {
    const bool high = ((col + step) & 3) < 2;
    c.fill((int16_t)(x + 1 + col), (int16_t)(kFlagY + (high ? 0 : 1)), 1, 4, high ? kGold : kOchre);
  }
  x = (int16_t)(x + 9 + 4 * (count - 1));
  for (uint8_t i = 0; i < count; i++, cm /= 10) tiny(c, (int16_t)(x - 4 * i), (int16_t)(kFlagY + 2), (uint8_t)(cm % 10), kGold);
  c.set((int16_t)(x + 4), (int16_t)(kFlagY + 6), kGold);
  tiny(c, (int16_t)(x + 6), (int16_t)(kFlagY + 2), (uint8_t)(mm % 10), kGold);
}

// Floppy: white silhouette when it pops in, then cyan with its shutter and a label written in two lines of 6 pixels.
void Scene::drawFloppy(Canvas& c, bool white, uint8_t written) const {
  if (white) {
    c.sprite(kFloppyX, kFloppyY, kFloppy, 12, 12, kWhite);
    return;
  }
  c.sprite(kFloppyX, kFloppyY, kFloppy, 12, 12, kCyan);
  c.sprite(kFloppyX, kFloppyY, kShutter, 12, 5, kSilver);
  c.sprite(kFloppyX, kFloppyY, kShutterHole, 12, 4, kNavy);
  c.fill(kFloppyX + 2, kFloppyY + 6, 8, 5, kCream);
  c.fill(kFloppyX + 3, kFloppyY + 7, written < 6 ? written : 6, 1, kDenim);
  if (written > 6) c.fill(kFloppyX + 3, kFloppyY + 9, (int16_t)(written - 6), 1, kDenim);
}
