#include "canvas.h"

namespace {

constexpr uint16_t rgb(uint32_t c) { return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F)); }

// Same order as enum Ink
const uint16_t kPalette[] = {
  rgb(0x0E1230), rgb(0x1E2552), rgb(0x2C3570), rgb(0x525EA0), rgb(0x8E98D0), rgb(0x3A4482),   // navy .. dusk
  rgb(0xFFF4E4), rgb(0xFFFFFF), rgb(0x8E86F5), rgb(0xD8D4FF), rgb(0x4842A0),                   // cream .. plum
  rgb(0x3EE07A), rgb(0xC4FFD2), rgb(0x1A7844), rgb(0xFF9A2E), rgb(0xFFE0AE), rgb(0x964C0E),   // green .. rust
  rgb(0xFFD447), rgb(0xFFF6C8), rgb(0xA07418), rgb(0x38C6FF), rgb(0xCDF2FF), rgb(0x185CA0),   // gold .. denim
  rgb(0xC6D0E8),                                                                              // silver
};
static_assert(sizeof(kPalette) / sizeof(kPalette[0]) == kInkCount, "one colour per Ink");

}  // namespace

Canvas::Canvas() {
  // A logical pixel is visible when its 4 x 4 block reaches the round panel: a screen pixel (x, y) is visible when
  // (x - 119.5)² + (y - 119.5)² <= 120². Doubled coordinates keep it in integers.
  for (int16_t y = 0; y < kSize; y++) {
    int32_t dy = y < kSize / 2 ? 233 - 8 * y : 8 * y - 239;      // nearest pixel row of the block, doubled
    int16_t x = 0;
    while (x < kSize / 2) {
      int32_t dx = 233 - 8 * x;
      if (dx * dx + dy * dy <= 4L * 120 * 120) break;
      x++;
    }
    _first[y] = (uint8_t)x;
  }
  clear(kNavy);
  invalidate();
}

uint16_t Canvas::color(uint8_t ink) { return ink < kInkCount ? kPalette[ink] : 0; }

void Canvas::fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ink) {
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) set((int16_t)(x + i), (int16_t)(y + j), ink);
  }
}

void Canvas::bits(int16_t x, int16_t y, uint16_t row, uint8_t w, uint8_t ink) {
  for (uint8_t i = 0; i < w; i++) {
    if (row & (1u << (w - 1 - i))) set((int16_t)(x + i), y, ink);
  }
}

void Canvas::sprite(int16_t x, int16_t y, const uint16_t* rows, uint8_t w, uint8_t h, uint8_t ink) {
  for (uint8_t j = 0; j < h; j++) bits(x, (int16_t)(y + j), rows[j], w, ink);
}

void Canvas::invalidate() { memset(_shown, kUnknown, sizeof(_shown)); }

// One window per run of changed pixels on a row: every changed logical pixel costs 32 bytes of SPI.
void Canvas::flush(Gc9a01& lcd) {
  uint16_t run[kSize];
  for (int16_t y = 0; y < kSize; y++) {
    const uint8_t* px = _px + y * kSize;
    uint8_t* shown = _shown + y * kSize;
    const int16_t last = (int16_t)(kSize - 1 - _first[y]);
    int16_t x = _first[y];
    while (x <= last) {
      if (px[x] == shown[x]) {
        x++;
        continue;
      }
      int16_t x0 = x;
      uint8_t n = 0;
      for (; x <= last && px[x] != shown[x]; x++) {
        run[n++] = kPalette[px[x]];
        shown[x] = px[x];
      }
      lcd.drawScaledRun((int16_t)(x0 * kScale), (int16_t)(y * kScale), run, n, kScale);
    }
  }
}
