#include "display.h"
#include "board_pins.h"
#include "config.h"
#include <avr/pgmspace.h>
#include <string.h>

namespace {

constexpr int16_t kCx = 120;
constexpr int16_t kCy = 120;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGrey = 0x8410;
constexpr uint16_t kDarkGrey = 0x3186;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kOrange = 0xFD20;
constexpr uint16_t kYellow = 0xFFE0;
constexpr uint16_t kRed = 0xF800;

// Gauge ring: 120 discs of radius 7 laid along a circle of radius 109, one every 3 degrees, clockwise from 12 o'clock.
constexpr int16_t kRingOuter = 116;
constexpr int16_t kRingInner = 102;
constexpr int16_t kRingMid = 109;
constexpr int16_t kRingDot = 7;
constexpr uint8_t kRingSegments = 120;
constexpr int16_t kRingStepDeg = 3;

// Main screen: arrow above, big "cm" digits, small tenth, unit below. The built-in font cell is 6x8 px x scale.
constexpr int16_t kArrowTop = 22;
constexpr int16_t kBigY = 84;
constexpr uint8_t kBigScale = 8;
constexpr uint8_t kTenthScale = 4;
constexpr int16_t kBigCell = 6 * kBigScale;                  // 48
constexpr int16_t kPointGap = 6;
constexpr int16_t kPointSize = 8;
constexpr int16_t kTenthCell = 6 * kTenthScale;              // 24
constexpr int16_t kBigBaseline = kBigY + 7 * kBigScale;      // bottom of the big glyphs

// sin(x) * 1024 for x = 0, 3, 6 ... 90 degrees
const int16_t kSin3[31] PROGMEM = {0, 54, 107, 160, 213, 265, 316, 367, 416, 465, 512, 558, 602, 644, 685, 724, 761, 796, 828, 859, 887, 912, 935, 956, 974, 989, 1002, 1011, 1018, 1023, 1024};

int16_t sinDeg(int16_t deg) {                                // deg: multiple of 3, any sign
  deg %= 360;
  if (deg < 0) deg += 360;
  bool negative = deg >= 180;
  if (negative) deg -= 180;
  if (deg > 90) deg = 180 - deg;
  int16_t v = (int16_t)pgm_read_word(&kSin3[deg / 3]);
  return negative ? -v : v;
}

// "72.5" – out needs 8 bytes
void formatCm(char* out, int16_t mm) {
  if (mm < 0) mm = 0;
  uint16_t cm = (uint16_t)(mm / 10);
  uint8_t tenth = (uint8_t)(mm % 10);
  char tmp[5];
  uint8_t n = 0;
  do {
    tmp[n++] = (char)('0' + cm % 10);
    cm /= 10;
  } while (cm && n < 4);
  uint8_t o = 0;
  while (n) out[o++] = tmp[--n];
  out[o++] = '.';
  out[o++] = (char)('0' + tenth);
  out[o] = 0;
}

}  // namespace

void Display::begin() {
  _lcd.begin(DISPLAY_ROTATION);
  _lcd.fillScreen(kBlack);
}

// ---------------------------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------------------------
void Display::centerP(const __FlashStringHelper* s, int16_t y, uint8_t scale, uint16_t color) {
  if (!s) return;
  int16_t w = (int16_t)strlen_P(reinterpret_cast<const char*>(s)) * 6 * scale;
  int16_t x = (240 - w) / 2;
  if (x < 0) x = 0;
  _lcd.drawTextP(x, y, s, scale, color, kBlack);
}

void Display::center(const char* s, int16_t y, uint8_t scale, uint16_t fg) {
  int16_t w = (int16_t)strlen(s) * 6 * scale;
  int16_t x = (240 - w) / 2;
  if (x < 0) x = 0;
  _lcd.drawText(x, y, s, scale, fg, kBlack);
}

// Paints the ring up to `segments` (monotonic during one gauge run).
void Display::drawRingTo(uint8_t segments, uint16_t color) {
  if (segments > kRingSegments) segments = kRingSegments;
  for (uint8_t k = _gaugeSegments; k < segments; k++) {
    int16_t deg = (int16_t)(k * kRingStepDeg);
    int16_t x = (int16_t)(kCx + (((int32_t)kRingMid * sinDeg(deg)) >> 10));
    int16_t y = (int16_t)(kCy - (((int32_t)kRingMid * sinDeg((int16_t)(deg + 90))) >> 10));
    _lcd.fillCircle(x, y, kRingDot, color);
  }
  if (segments > _gaugeSegments) _gaugeSegments = segments;
}

// Thick line made of discs (used for the check mark).
void Display::stampLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t radius, uint16_t color) {
  int16_t dx = (int16_t)(x1 - x0);
  int16_t dy = (int16_t)(y1 - y0);
  int16_t n = (int16_t)((abs(dx) > abs(dy) ? abs(dx) : abs(dy)) / 3);
  if (n < 1) n = 1;
  for (int16_t i = 0; i <= n; i++) {
    _lcd.fillCircle((int16_t)(x0 + (int32_t)dx * i / n), (int16_t)(y0 + (int32_t)dy * i / n), radius, color);
  }
}

// ---------------------------------------------------------------------------------------------
//  Screens
// ---------------------------------------------------------------------------------------------
void Display::drawMainStatic() { centerP(F("cm"), 160, 3, kGrey); }

void Display::drawArrow(UiModel::Arrow a) {
  _lcd.fillRect(kCx - 30, kArrowTop - 2, 61, 60, kBlack);
  if (a == UiModel::Arrow::Up) {
    for (int16_t r = 0; r < 30; r++) {                       // head: widens downwards
      int16_t half = (int16_t)(1 + (27 * r) / 29);
      _lcd.fillRect((int16_t)(kCx - half), (int16_t)(kArrowTop + r), (int16_t)(2 * half + 1), 1, kGreen);
    }
    _lcd.fillRect(kCx - 10, kArrowTop + 30, 21, 26, kGreen);
  } else if (a == UiModel::Arrow::Down) {
    _lcd.fillRect(kCx - 10, kArrowTop, 21, 26, kOrange);
    for (int16_t r = 0; r < 30; r++) {                       // head: narrows downwards
      int16_t half = (int16_t)(28 - (27 * r) / 29);
      _lcd.fillRect((int16_t)(kCx - half), (int16_t)(kArrowTop + 26 + r), (int16_t)(2 * half + 1), 1, kOrange);
    }
  }
  _arrowShown = a;
}

// Big integer cm digits, then a small point and tenth ("72" "." "5").
void Display::drawNumber(const UiModel& m) {
  char big[4] = {0, 0, 0, 0};
  char tenth;
  uint8_t digits;
  if (!m.heightValid) {
    big[0] = big[1] = '-';
    digits = 2;
    tenth = '-';
  } else {
    int16_t h = m.heightMm < 0 ? 0 : m.heightMm;
    if (h > 9999) h = 9999;
    uint16_t cm = (uint16_t)(h / 10);
    tenth = (char)('0' + h % 10);
    digits = cm >= 100 ? 3 : (cm >= 10 ? 2 : 1);
    for (int8_t i = (int8_t)digits - 1; i >= 0; i--) {
      big[i] = (char)('0' + cm % 10);
      cm /= 10;
    }
  }

  bool force = !_numberDrawn || digits != _bigDigits || m.heightValid != _numberValid;
  if (force) _lcd.fillRect(0, kBigY - 4, 240, 7 * kBigScale + 8, kBlack);   // the block is re-centred when its width changes

  int16_t blockW = (int16_t)(digits * kBigCell + kPointGap + kPointSize + kPointGap + kTenthCell);
  int16_t x0 = (int16_t)((240 - blockW) / 2);
  uint16_t color = m.heightValid ? kWhite : kGrey;

  for (uint8_t i = 0; i < digits; i++) {
    if (force || big[i] != _bigShown[i]) {
      _lcd.drawChar((int16_t)(x0 + i * kBigCell), kBigY, big[i], kBigScale, color, kBlack);
      _bigShown[i] = big[i];
    }
  }
  int16_t xPoint = (int16_t)(x0 + digits * kBigCell + kPointGap);
  if (force) _lcd.fillRect(xPoint, (int16_t)(kBigBaseline - kPointSize), kPointSize, kPointSize, color);
  if (force || tenth != _tenthShown) {
    _lcd.drawChar((int16_t)(xPoint + kPointSize + kPointGap), (int16_t)(kBigBaseline - 7 * kTenthScale), tenth, kTenthScale, color, kBlack);
    _tenthShown = tenth;
  }

  _bigDigits = digits;
  _numberDrawn = true;
  _numberValid = m.heightValid;
  _numberMm = m.heightMm;
}

void Display::drawSaveHeight(const UiModel& m) {
  char buf[8];
  char field[10];
  if (m.heightValid) formatCm(buf, m.heightMm);
  else strcpy(buf, "--.-");
  // fixed 8-character field, text centred, so a shorter value fully overwrites a longer one
  uint8_t len = (uint8_t)strlen(buf);
  uint8_t pad = (uint8_t)((8 - len) / 2);
  memset(field, ' ', 8);
  memcpy(field + pad, buf, len);
  field[8] = 0;
  center(field, 98, 4, kWhite);
  _saveHeightShown = m.heightMm;
}

// Erase only what the main screen draws (the rest of the disc is already black): ~10x cheaper than a full clear.
void Display::clearMainContent() {
  _lcd.fillRect(0, kBigY - 4, 240, 7 * kBigScale + 8, kBlack);            // number band
  _lcd.fillRect(kCx - 30, kArrowTop - 2, 61, 60, kBlack);                 // arrow box
  _lcd.fillRect(kCx - 20, 158, 40, 28, kBlack);                           // "cm"
}

// Same for the three text rows of the gauge screen.
void Display::clearGaugeTexts() {
  _lcd.fillRect(48, 50, 144, 28, kBlack);                                 // PRESET n   (kept inside the ring's hole)
  _lcd.fillRect(26, 96, 188, 36, kBlack);                                 // height
  _lcd.fillRect(48, 164, 144, 20, kBlack);                                // HOLD TO SAVE
}

void Display::drawCheckMark() {
  stampLine(88, 88, 104, 104, 5, kGreen);
  stampLine(104, 104, 150, 58, 5, kGreen);
}

void Display::enterScreen(const UiModel& m) {
  UiModel::Screen previous = _screen;
  bool hadPrevious = _screenValid;

  switch (m.screen) {
    case UiModel::Screen::Boot:
      _lcd.fillScreen(kBlack);
      centerP(m.title, 92, 3, kWhite);
      centerP(m.subtitle, 134, 2, kGrey);
      break;

    case UiModel::Screen::Main:
      _lcd.fillScreen(kBlack);
      drawMainStatic();
      _arrowShown = UiModel::Arrow::None;
      _numberDrawn = false;
      break;

    case UiModel::Screen::SaveGauge: {
      if (hadPrevious && previous == UiModel::Screen::Main) clearMainContent();
      else _lcd.fillScreen(kBlack);
      _lcd.fillRing(kCx, kCy, kRingOuter, kRingInner, kDarkGrey);
      _gaugeSegments = 0;
      char title[10] = "PRESET  ";
      title[7] = (char)('0' + m.presetNumber);
      center(title, 52, 3, kWhite);
      centerP(F("HOLD TO SAVE"), 166, 2, kGrey);
      drawSaveHeight(m);
      break;
    }

    case UiModel::Screen::SaveSuccess: {
      if (hadPrevious && previous == UiModel::Screen::SaveGauge) {
        clearGaugeTexts();                                   // keep the ring: it simply completes
        drawRingTo(kRingSegments, kGreen);
      } else {
        _lcd.fillScreen(kBlack);
        _lcd.fillRing(kCx, kCy, kRingOuter, kRingInner, kGreen);
      }
      drawCheckMark();
      centerP(F("SUCCESS"), 116, 4, kGreen);
      char line[10] = "PRESET  ";
      line[7] = (char)('0' + m.presetNumber);
      line[8] = 0;
      center(line, 150, 2, kGrey);
      char buf[8];
      char text[12];
      formatCm(buf, m.savedHeightMm);
      strcpy(text, buf);
      strcat(text, " cm");
      center(text, 168, 3, kWhite);
      break;
    }

    case UiModel::Screen::Notice:
      _lcd.fillScreen(kBlack);
      centerP(m.title, 92, 3, kYellow);
      centerP(m.subtitle, 134, 2, kGrey);
      break;

    case UiModel::Screen::Fault:
      _lcd.fillScreen(kBlack);
      _lcd.fillCircle(kCx, 52, 14, kRed);                    // warning disc with an exclamation mark
      _lcd.fillRect(kCx - 3, 43, 6, 14, kBlack);
      _lcd.fillRect(kCx - 3, 60, 6, 6, kBlack);
      centerP(m.title, 92, 3, kRed);
      centerP(m.subtitle, 134, 2, kGrey);
      break;
  }

  _screen = m.screen;
  _screenValid = true;
  _presetShown = m.presetNumber;
  _titleShown = m.title;
  _subtitleShown = m.subtitle;
}

void Display::updateScreen(const UiModel& m) {
  switch (m.screen) {
    case UiModel::Screen::Main: {
      bool moving = m.arrow != UiModel::Arrow::None;
      int16_t diff = (int16_t)(m.heightMm - _numberMm);
      if (diff < 0) diff = (int16_t)-diff;
      bool changed = !_numberDrawn || m.heightValid != _numberValid ||
                     (m.heightValid && diff >= (moving ? 1 : DISPLAY_DEADBAND_MM));
      if (changed) drawNumber(m);
      if (m.arrow != _arrowShown) drawArrow(m.arrow);
      break;
    }
    case UiModel::Screen::SaveGauge: {
      int16_t diff = (int16_t)(m.heightMm - _saveHeightShown);
      if (diff < 0) diff = (int16_t)-diff;
      if (diff >= DISPLAY_DEADBAND_MM) drawSaveHeight(m);
      drawRingTo((uint8_t)(((uint16_t)m.gaugePercent * kRingSegments) / 100), kGreen);
      break;
    }
    default:
      break;                                                 // static screens
  }
}

void Display::render(const UiModel& m, uint32_t nowMs) {
  bool screenChanged = !_screenValid || m.screen != _screen ||
                       (m.screen == UiModel::Screen::SaveGauge && m.presetNumber != _presetShown) ||
                       ((m.screen == UiModel::Screen::Notice || m.screen == UiModel::Screen::Fault ||
                         m.screen == UiModel::Screen::Boot) &&
                        (m.title != _titleShown || m.subtitle != _subtitleShown));
  if (!screenChanged && (uint32_t)(nowMs - _lastRender) < DISPLAY_MIN_PERIOD_MS) return;
  _lastRender = nowMs;
  if (screenChanged) enterScreen(m);
  updateScreen(m);
}
