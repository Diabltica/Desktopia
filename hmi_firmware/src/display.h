#pragma once
#include <Arduino.h>
#include "gc9a01.h"
#include "ui_model.h"

// Round 240x240 screen UI. Stateless from the caller's point of view: hand it the UiModel every loop,
// it redraws only what changed. Runs on core 1 of the RP2040; the content comes from the motherboard.
class Display {
 public:
  void begin();
  void render(const UiModel& model, uint32_t nowMs);

 private:
  void enterScreen(const UiModel& m);
  void updateScreen(const UiModel& m);

  void drawMainStatic();
  void drawNumber(const UiModel& m);
  void drawArrow(UiModel::Arrow a);
  void drawSaveHeight(const UiModel& m);
  void drawCheckMark();
  void clearMainContent();
  void clearGaugeTexts();
  void drawRingTo(uint8_t segments, uint16_t color);
  void stampLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t radius, uint16_t color);
  void centerT(UiText t, int16_t y, uint8_t scale, uint16_t color);
  void center(const char* s, int16_t y, uint8_t scale, uint16_t fg);

  Gc9a01 _lcd;
  bool _screenValid = false;
  UiModel::Screen _screen = UiModel::Screen::Boot;
  uint8_t _presetShown = 0;
  UiText _titleShown = UiText::None;
  UiText _subtitleShown = UiText::None;
  uint32_t _lastRender = 0;

  // Main screen cache
  UiModel::Arrow _arrowShown = UiModel::Arrow::None;
  bool _numberDrawn = false;
  bool _numberValid = false;
  int16_t _numberMm = 0;
  uint8_t _bigDigits = 0;
  char _bigShown[4] = {0, 0, 0, 0};
  char _tenthShown = 0;

  // Gauge cache
  uint8_t _gaugeSegments = 0;
  int16_t _saveHeightShown = -1;
};
