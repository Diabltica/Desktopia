#pragma once
#include <Arduino.h>

// Compact driver for 240x240 round GC9A01 panels, hardware SPI, write-only, RGB565.
//
// Written for the ATmega32U4 of the first board revision (no room for Adafruit_GFX); kept on the RP2040 because
// it does exactly what the UI needs (rectangles, discs, scaled 5x7 text) and the screens look the same.
// The panel init sequence and the rotation values are taken from Adafruit_GC9A01A
// (BSD licence, (c) Adafruit Industries) – it is the sequence known to work on round GC9A01 modules.
class Gc9a01 {
 public:
  enum : int16_t { kSize = 240 };

  // Pins come from hmi_pins.h; SPI0 of the RP2040. Blocking (~0.7 s, reset + init sequence).
  void begin(uint8_t rotation);

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);   // clipped to the screen
  void fillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
  void fillRing(int16_t cx, int16_t cy, int16_t rOuter, int16_t rInner, uint16_t color);
  void fillScreen(uint16_t color) { fillCircle(kSize / 2, kSize / 2, kSize / 2 + 1, color); }   // the panel is round: only the visible disc (-22 % pixels)

  // Built-in 5x7 font (ASCII 32..126) drawn in a 6x8 cell x scale, background painted (opaque).
  void drawChar(int16_t x, int16_t y, char c, uint8_t scale, uint16_t fg, uint16_t bg);
  void drawText(int16_t x, int16_t y, const char* text, uint8_t scale, uint16_t fg, uint16_t bg);

 private:
  void sendCommand(uint8_t cmd, const uint8_t* args, uint8_t count);
  void openWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1);        // CS low, window set, RAMWR sent
  void closeWindow();                                                     // CS high
  void streamColor(uint16_t color, uint32_t count);
};
