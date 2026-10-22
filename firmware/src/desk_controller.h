#pragma once
#include <Arduino.h>
#include "buttons.h"
#include "current_monitor.h"
#include "hmi_link.h"
#include "motor.h"
#include "presets.h"
#include "tof_sensor.h"
#include "ui_model.h"

// Application logic: turns button events + measured height into motor commands and screen content.
//
//  Idle ──Up/Down held──▶ Manual ──button released / limit──▶ Idle
//  Idle ──preset short press──▶ GotoPreset ──target reached / any button (Up + Down)──▶ Idle, inputs ignored until release
//  Idle ──preset held 400 ms──▶ Saving ──held 2 s: save──▶ Idle (+ success overlay) / released: cancel
//  any state ──sensor lost while moving, stall, wrong way, timeout, watchdog──▶ Fault ──cleared──▶ Idle
//  any state ──motor current alert (INA226, driver supply already cut by INT6)──▶ Fault ──cleared──▶ Idle
//  any state ──HMI link lost (cord unplugged: the HMI board is unpowered)──▶ Fault ──link back──▶ Idle (no fault screen)
// The motor module switches the driver supply on for a move and off after MOTOR_POWER_IDLE_OFF_MS of rest.
class DeskController {
 public:
  DeskController(HmiLink& link, Buttons& buttons, TofSensor& tof, Motor& motor, Presets& presets, CurrentMonitor& current)
      : _link(link), _buttons(buttons), _tof(tof), _motor(motor), _presets(presets), _current(current) {}

  // watchdogReset: the previous run ended in a watchdog reset.
  void begin(uint32_t nowMs, bool watchdogReset);
  void update(uint32_t nowMs);

  const UiModel& ui() const { return _ui; }

 private:
  enum class State : uint8_t { Idle, Manual, GotoPreset, Saving, Fault };
  enum class Overlay : uint8_t { None, Notice, Success };
  enum class FaultCode : uint8_t { None, Link, Sensor, WrongWay, Stall, Timeout, Watchdog, Overcurrent, CurrentSense };

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
  bool currentMonitorReady(uint32_t nowMs);             // false = a notice was shown, no move
  bool monitorMove(int16_t heightMm, uint32_t nowMs);   // false = a fault was raised
  void logCurrent(uint32_t nowMs);
  void enterFault(FaultCode code, uint32_t nowMs);
  void showNotice(UiText title, UiText subtitle, uint32_t nowMs);
  void refreshUi(uint32_t nowMs);

  static uint16_t approachSpeedPermille(int16_t remainingMm);
  static bool isPresetButton(Button b) { return b >= Button::Preset1; }
  static uint8_t presetIndexOf(Button b) { return (uint8_t)((uint8_t)b - (uint8_t)Button::Preset1); }

  HmiLink& _link;
  Buttons& _buttons;
  TofSensor& _tof;
  Motor& _motor;
  Presets& _presets;
  CurrentMonitor& _current;

  State _state = State::Idle;
  FaultCode _fault = FaultCode::None;
  uint32_t _stateSince = 0;
  uint32_t _bootUntil = 0;            // Boot screen until then (TOF_BYPASS warning)

  // movement bookkeeping
  Motor::Direction _dir = Motor::Direction::Up;
  Button _holdButton = Button::Up;
  bool _waitRelease = false;           // a goto was cancelled: nothing starts before every button is released
  int16_t _target = 0;
  uint32_t _moveStart = 0;
  int16_t _moveStartHeight = 0;
  int16_t _progressHeight = 0;
  uint16_t _stallMs = 0;
  uint32_t _lastMonitor = 0;
  uint32_t _lastCurrentLog = 0;

  // preset saving
  Button _saveButton = Button::Preset1;
  uint8_t _presetIndex = 0;

  // transient messages shown on top of the Idle state
  Overlay _overlay = Overlay::None;
  uint32_t _overlayUntil = 0;
  UiText _noticeTitle = UiText::None;
  UiText _noticeSubtitle = UiText::None;
  uint8_t _successPreset = 1;
  int16_t _successHeight = 0;

  UiModel _ui;
};
