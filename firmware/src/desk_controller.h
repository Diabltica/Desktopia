#pragma once
#include <Arduino.h>
#include "buttons.h"
#include "io_expander.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "ui_model.h"

// Application logic: turns button events + measured height into motor commands and screen content.
//
//  Idle ──Up/Down held──▶ Manual ──button released / limit──▶ Idle
//  Idle ──preset short press──▶ GotoPreset ──target reached / any button──▶ Idle
//  Idle ──preset held 400 ms──▶ Saving ──held 2 s: save──▶ Idle (+ success overlay) / released: cancel
//  any state ──sensor lost while moving, stall, wrong way, timeout, watchdog──▶ Fault ──cleared──▶ Idle
class DeskController {
 public:
  DeskController(IoExpander& io, Buttons& buttons, TofSensor& tof, Motor& motor, Presets& presets)
      : _io(io), _buttons(buttons), _tof(tof), _motor(motor), _presets(presets) {}

  // ioOk: result of the expander init, watchdogReset: the previous run ended in a watchdog reset.
  void begin(uint32_t nowMs, bool ioOk, bool watchdogReset);
  void update(uint32_t nowMs);

  const UiModel& ui() const { return _ui; }

 private:
  enum class State : uint8_t { Idle, Manual, GotoPreset, Saving, Fault };
  enum class Overlay : uint8_t { None, Notice, Success };
  enum class FaultCode : uint8_t { None, ButtonIo, Sensor, WrongWay, Stall, Timeout, Watchdog };

  void handleEvent(const ButtonEvent& ev, uint32_t nowMs);

  void updateIdle(uint32_t nowMs);
  void updateManual(uint32_t nowMs);
  void updateGoto(uint32_t nowMs);
  void updateSaving(uint32_t nowMs);
  void updateFault(uint32_t nowMs);

  void startManual(Button b, uint32_t nowMs);
  void startGoto(uint8_t presetIndex, uint32_t nowMs);
  void startSaving(Button b, uint32_t nowMs);
  void finishSaving(uint32_t nowMs);

  void beginMove(uint32_t nowMs);
  bool monitorMove(int16_t heightMm, uint32_t nowMs);   // false = a fault was raised
  void enterFault(FaultCode code, uint32_t nowMs);
  void showNotice(const __FlashStringHelper* title, const __FlashStringHelper* subtitle, uint32_t nowMs);
  void refreshUi(uint32_t nowMs);

  static uint16_t approachSpeedPermille(int16_t remainingMm);
  static bool isPresetButton(Button b) { return b >= Button::Preset1; }
  static uint8_t presetIndexOf(Button b) { return (uint8_t)((uint8_t)b - (uint8_t)Button::Preset1); }

  IoExpander& _io;
  Buttons& _buttons;
  TofSensor& _tof;
  Motor& _motor;
  Presets& _presets;

  State _state = State::Idle;
  FaultCode _fault = FaultCode::None;
  uint32_t _stateSince = 0;
  uint32_t _lastRecover = 0;

  // movement bookkeeping
  Motor::Direction _dir = Motor::Direction::Up;
  Button _holdButton = Button::Up;
  int16_t _target = 0;
  uint32_t _moveStart = 0;
  int16_t _moveStartHeight = 0;
  int16_t _progressHeight = 0;
  uint16_t _stallMs = 0;
  uint32_t _lastMonitor = 0;

  // preset saving
  Button _saveButton = Button::Preset1;
  uint8_t _presetIndex = 0;

  // transient messages shown on top of the Idle state
  Overlay _overlay = Overlay::None;
  uint32_t _overlayUntil = 0;
  const __FlashStringHelper* _noticeTitle = nullptr;
  const __FlashStringHelper* _noticeSubtitle = nullptr;
  uint8_t _successPreset = 1;
  int16_t _successHeight = 0;

  UiModel _ui;
};
