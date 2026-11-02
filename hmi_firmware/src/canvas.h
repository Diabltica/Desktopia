#pragma once
#include <Arduino.h>
#include <string.h>
#include "gc9a01.h"

// Palette of the pixel-art screens (RGB565 values in canvas.cpp). Named after the colour, commented with its main use.
enum Ink : uint8_t {
  kNavy,       // background
  kTrack,      // empty part of the ring
  kShadow,     // drop shadow of the numbers
  kSteel,      // pill outline, preset mark on the ring
  kMist,       // "cm", pill digit, flag pole
  kDusk,       // pill of an empty preset (dotted)
  kCream,      // numbers
  kWhite,      // flash, core of a sparkle, ring shine
  kLavender,   // ring at rest, "Z"
  kLilac,      // head of the ring at rest, newborn "z"
  kPlum,       // fading "z"
  kGreen,      // going up
  kMint,       // going up: ring head, bubbles, nearest chevrons
  kForest,     // going up: fading chevrons
  kOrange,     // going down
  kPeach,      // going down: ring head, bubbles, nearest chevrons
  kRust,       // going down: fading chevrons
  kGold,       // presets: the one the desk is on, arrival, saved
  kPaleGold,   // mark of the target during an automatic move, head of a gold ring
  kOchre,      // travel left (dots), flag shade
  kCyan,       // saving
  kIce,        // saving: newest gauge segment
  kDenim,      // saving: lines written on the floppy label
  kSilver,     // floppy shutter
  kInkCount
};

// 60 x 60 frame of "logical pixels" for the pixel-art screens: one logical pixel is a 4 x 4 block of the panel and
// holds an Ink. A frame is always drawn from scratch into RAM; flush() then sends only the logical pixels that differ
// from what the panel shows, and only those of the round visible area, so a frame where little moves costs little SPI.
class Canvas {
 public:
  enum : int16_t { kSize = 60, kScale = Gc9a01::kSize / kSize };

  Canvas();
  static uint16_t color(uint8_t ink);              // RGB565

  void clear(uint8_t ink) { memset(_px, ink, sizeof(_px)); }
  void set(int16_t x, int16_t y, uint8_t ink) {    // clipped
    if ((uint16_t)x < kSize && (uint16_t)y < kSize) _px[y * kSize + x] = ink;
  }
  void fill(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t ink);
  // One row of a 1-bit sprite: w pixels, the leftmost in bit w - 1. Only the set bits are drawn.
  void bits(int16_t x, int16_t y, uint16_t row, uint8_t w, uint8_t ink);
  void sprite(int16_t x, int16_t y, const uint16_t* rows, uint8_t w, uint8_t h, uint8_t ink);

  void invalidate();                               // panel content unknown: the next flush() sends the whole disc
  void flush(Gc9a01& lcd);

 private:
  enum : uint8_t { kUnknown = 0xFF };

  uint8_t _px[kSize * kSize];
  uint8_t _shown[kSize * kSize];                   // what the panel shows
  uint8_t _first[kSize];                           // visible logical pixels of each row: _first[y] .. kSize - 1 - _first[y]
};
