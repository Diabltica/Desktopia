#pragma once
#include <Arduino.h>
#include "canvas.h"
#include "gc9a01.h"
#include "scene.h"
#include "ui_model.h"

// Round 240x240 screen UI. Stateless from the caller's point of view: hand it the UiModel every loop,
// it redraws only what changed. Runs on core 1 of the RP2040; the content comes from the motherboard.
//
// Main, SaveGauge, SaveSuccess and Arrived are animated pixel art (scene.h, drawn through a 60 x 60 canvas);
// Boot, Notice and Fault are plain text screens in the same colours.
class Display {
 public:
  void begin();
  void render(const UiModel& model, uint32_t nowMs);

 private:
  static bool isPixelArt(UiModel::Screen s);
  void drawTextScreen(const UiModel& m);
  void centerT(UiText t, int16_t y, uint8_t scale, uint16_t color);
  void center(const char* s, int16_t y, uint8_t scale, uint16_t fg);

  Gc9a01 _lcd;
  Canvas _canvas;
  Scene _scene;
  bool _screenValid = false;
  UiModel::Screen _screen = UiModel::Screen::Boot;
  UiText _titleShown = UiText::None;
  UiText _subtitleShown = UiText::None;
  uint32_t _lastRender = 0;
};
