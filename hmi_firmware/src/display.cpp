#include "display.h"
#include "hmi_config.h"
#include "ui_text.h"
#include <string.h>

namespace {

constexpr int16_t kCx = 120;
constexpr uint16_t kRed = 0xF800;

uint16_t ink(uint8_t i) { return Canvas::color(i); }

}  // namespace

void Display::begin() {
  _lcd.begin(DISPLAY_ROTATION);
  _lcd.fillScreen(ink(kNavy));
  _scene.begin();
}

bool Display::isPixelArt(UiModel::Screen s) {
  return s == UiModel::Screen::Main || s == UiModel::Screen::SaveGauge || s == UiModel::Screen::SaveSuccess ||
         s == UiModel::Screen::Arrived;
}

// ---------------------------------------------------------------------------------------------
//  Text screens
// ---------------------------------------------------------------------------------------------
void Display::centerT(UiText t, int16_t y, uint8_t scale, uint16_t color) {
  if (t != UiText::None) center(uiText(t), y, scale, color);
}

void Display::center(const char* s, int16_t y, uint8_t scale, uint16_t fg) {
  int16_t w = (int16_t)strlen(s) * 6 * scale;
  int16_t x = (240 - w) / 2;
  if (x < 0) x = 0;
  _lcd.drawText(x, y, s, scale, fg, ink(kNavy));
}

void Display::drawTextScreen(const UiModel& m) {
  _lcd.fillScreen(ink(kNavy));
  switch (m.screen) {
    case UiModel::Screen::Boot:
      centerT(m.title, 92, 3, ink(kCream));
      centerT(m.subtitle, 134, 2, ink(kMist));
      break;

    case UiModel::Screen::Notice:
      centerT(m.title, 92, 3, ink(kGold));
      centerT(m.subtitle, 134, 2, ink(kMist));
      break;

    case UiModel::Screen::Fault:
      _lcd.fillCircle(kCx, 52, 14, kRed);                    // warning disc with an exclamation mark
      _lcd.fillRect(kCx - 3, 43, 6, 14, ink(kNavy));
      _lcd.fillRect(kCx - 3, 60, 6, 6, ink(kNavy));
      centerT(m.title, 92, 3, kRed);
      centerT(m.subtitle, 134, 2, ink(kMist));
      break;

    default:                                                 // pixel-art screens: see render()
      break;
  }
}

// ---------------------------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------------------------
void Display::render(const UiModel& m, uint32_t nowMs) {
  const bool pixelArt = isPixelArt(m.screen);
  const bool changed = !_screenValid || m.screen != _screen ||
                       (!pixelArt && (m.title != _titleShown || m.subtitle != _subtitleShown));
  if (!changed && (uint32_t)(nowMs - _lastRender) < DISPLAY_MIN_PERIOD_MS) return;
  _lastRender = nowMs;

  if (pixelArt) {
    if (!_screenValid || !isPixelArt(_screen)) {             // coming from a text screen: whole disc, animations over
      _canvas.invalidate();
      _scene.reset();
    }
    _scene.draw(_canvas, m, nowMs);
    _canvas.flush(_lcd);
  } else if (changed) {
    drawTextScreen(m);
  }

  _screen = m.screen;
  _screenValid = true;
  _titleShown = m.title;
  _subtitleShown = m.subtitle;
}
