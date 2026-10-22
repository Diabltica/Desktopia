#include "desk_controller.h"
#include "config.h"
#include "debug.h"

static_assert(HEIGHT_MIN_MM < HEIGHT_MAX_MM, "HEIGHT_MIN_MM must be below HEIGHT_MAX_MM");
static_assert(PRESET_SHORT_PRESS_MAX_MS < PRESET_LONG_PRESS_MS, "long press must be longer than the short press window");
static_assert(PRESET_COUNT >= 1 && PRESET_COUNT <= 3, "three preset buttons are wired");

#if TOF_BYPASS
#warning "TOF_BYPASS=1: height sensor ignored, no end stops - bench test only, motor decoupled from the desk"
#endif

namespace {
constexpr bool kTofBypass = TOF_BYPASS != 0;
constexpr bool kCurrentMonitorRequired = CURRENT_MONITOR_REQUIRED != 0;
constexpr uint16_t kMaxPermille = MOTOR_PWM_MAX_PERCENT * 10;
constexpr uint16_t kMinPermille = MOTOR_PWM_MIN_PERCENT * 10;

int16_t clampHeight(int16_t h) {
  if (h < HEIGHT_MIN_MM) return HEIGHT_MIN_MM;
  if (h > HEIGHT_MAX_MM) return HEIGHT_MAX_MM;
  return h;
}
}  // namespace

// ---------------------------------------------------------------------------------------------
//  Speed profile: full speed far away, linear slow-down inside APPROACH_DISTANCE_MM, floor at MIN.
//  The motor module then ramps the real duty towards this target (soft start / soft stop).
// ---------------------------------------------------------------------------------------------
uint16_t DeskController::approachSpeedPermille(int16_t remainingMm) {
  if (remainingMm <= 0) return 0;
  if (remainingMm >= APPROACH_DISTANCE_MM) return kMaxPermille;
  return (uint16_t)(kMinPermille + (uint32_t)(kMaxPermille - kMinPermille) * (uint16_t)remainingMm / APPROACH_DISTANCE_MM);
}

void DeskController::begin(uint32_t nowMs, bool watchdogReset) {
  _buttons.reset();
  _state = State::Idle;
  _stateSince = nowMs;
  _bootUntil = nowMs + BOOT_SCREEN_MS;
  if (watchdogReset) enterFault(FaultCode::Watchdog, nowMs);
  refreshUi(nowMs);
}

// ---------------------------------------------------------------------------------------------
//  Main entry point, once per loop()
// ---------------------------------------------------------------------------------------------
void DeskController::update(uint32_t nowMs) {
  _buttons.update(_link, nowMs);
  _tof.allowReinit(!_motor.isRunning());
  _tof.update(nowMs);
  _current.allowReinit(!_motor.isRunning());
  _current.update(nowMs);
  _motor.update(nowMs);

  // Safety checks that apply in every state
  if (_current.consumeAlert()) {                     // INT6 has already cut the PWM and the driver supply
    _motor.cutPower();
    enterFault(FaultCode::Overcurrent, nowMs);
  }
  if (kCurrentMonitorRequired && !_current.present() && _motor.isRunning() && _state != State::Fault) {
    enterFault(FaultCode::CurrentSense, nowMs);      // no current protection any more: stop
  }
  if (_motor.consumeFailsafeFlag() && _state != State::Fault) enterFault(FaultCode::Watchdog, nowMs);
  if (_buttons.ioError() && _fault != FaultCode::Link) enterFault(FaultCode::Link, nowMs);   // cannot see a release → stop
  logCurrent(nowMs);

  ButtonEvent ev;
  while (_buttons.nextEvent(ev)) handleEvent(ev, nowMs);

  switch (_state) {
    case State::Idle:       updateIdle(nowMs); break;
    case State::Manual:     updateManual(nowMs); break;
    case State::GotoPreset: updateGoto(nowMs); break;
    case State::Saving:     updateSaving(nowMs); break;
    case State::Fault:      updateFault(nowMs); break;
  }

  if (_overlay != Overlay::None && (int32_t)(nowMs - _overlayUntil) >= 0) _overlay = Overlay::None;
  refreshUi(nowMs);
}

// ---------------------------------------------------------------------------------------------
//  Button events
// ---------------------------------------------------------------------------------------------
void DeskController::handleEvent(const ButtonEvent& ev, uint32_t nowMs) {
  switch (_state) {
    case State::Idle:
      if (!isPresetButton(ev.button)) break;                                   // Up/Down are handled by level
      if (_buttons.isActive(Button::Up) || _buttons.isActive(Button::Down)) break;
      if (ev.type == ButtonEventType::ShortPress) startGoto(presetIndexOf(ev.button), nowMs);
      else if (ev.type == ButtonEventType::LongPressStart) startSaving(ev.button, nowMs);
      break;

    case State::Manual:
      if (ev.type == ButtonEventType::Pressed && isPresetButton(ev.button)) _buttons.consume(ev.button);
      break;

    case State::GotoPreset:
      if (ev.type == ButtonEventType::Pressed) {                               // any press (e.g. Up + Down) cancels the move
        _motor.stop();
        _buttons.consume(ev.button);
        _waitRelease = true;                                                   // the other button of the pair must not start a move
        _state = State::Idle;
        _stateSince = nowMs;
      }
      break;

    case State::Saving:
      if (ev.button == _saveButton) {
        if (ev.type == ButtonEventType::LongPressDone) finishSaving(nowMs);
        else if (ev.type == ButtonEventType::LongPressAbort) { _state = State::Idle; _stateSince = nowMs; }
      } else if (ev.type == ButtonEventType::Pressed) {
        _buttons.consume(ev.button);
      }
      break;

    case State::Fault:
      if (ev.type == ButtonEventType::Pressed) _buttons.consume(ev.button);
      break;
  }
}

// ---------------------------------------------------------------------------------------------
//  Idle
// ---------------------------------------------------------------------------------------------
void DeskController::updateIdle(uint32_t nowMs) {
  if (_waitRelease) {                                                          // after a cancelled goto: ignore every press
    for (uint8_t i = 0; i < Buttons::kCount; i++) {                            // until all the buttons are released
      if (_buttons.isPressed(static_cast<Button>(i))) _buttons.consume(static_cast<Button>(i));
    }
    _waitRelease = _buttons.anyPressed();
    return;
  }
  bool up = _buttons.isActive(Button::Up);
  bool down = _buttons.isActive(Button::Down);
  if (up && down) {                                                            // ambiguous: ignore both until released
    _buttons.consume(Button::Up);
    _buttons.consume(Button::Down);
    return;
  }
  if (up || down) startManual(up ? Button::Up : Button::Down, nowMs);
}

void DeskController::beginMove(uint32_t nowMs) {
  _moveStart = nowMs;
  _moveStartHeight = _tof.heightMm();
  _progressHeight = _moveStartHeight;
  _stallMs = 0;
  _lastMonitor = nowMs;
  _stateSince = nowMs;
  _overlay = Overlay::None;
}

// The INA226 is the only thing that cuts the motor on an overcurrent: no move without it (CURRENT_MONITOR_REQUIRED).
bool DeskController::currentMonitorReady(uint32_t nowMs) {
  if (!kCurrentMonitorRequired || _current.present()) return true;
  showNotice(UiText::NoCurrentSense, UiText::Ina226NotAnswering, nowMs);
  return false;
}

void DeskController::logCurrent(uint32_t nowMs) {
#if DEBUG_SERIAL
  if (!_motor.isPowered() || (uint32_t)(nowMs - _lastCurrentLog) < 250) return;
  _lastCurrentLog = nowMs;
  int32_t ma;
  if (_current.readCurrentMa(ma)) {
    DBG(F("motor mA "));
    DBGLN(ma);
  }
#else
  (void)nowMs;
#endif
}

void DeskController::startManual(Button b, uint32_t nowMs) {
  if (!currentMonitorReady(nowMs)) {
    _buttons.consume(b);
    return;
  }
  if (kTofBypass) {                                                            // bench test: no height, no end stops
    _holdButton = b;
    _dir = (b == Button::Up) ? Motor::Direction::Up : Motor::Direction::Down;
    beginMove(nowMs);
    _state = State::Manual;
    return;
  }
  if (!_tof.hasFreshHeight(nowMs)) {
    _buttons.consume(b);
    showNotice(UiText::NoSensor, UiText::HeightUnknown, nowMs);
    return;
  }
  Motor::Direction dir = (b == Button::Up) ? Motor::Direction::Up : Motor::Direction::Down;
  int16_t h = _tof.heightMm();
  int16_t remaining = (dir == Motor::Direction::Up) ? (int16_t)(HEIGHT_MAX_MM - h) : (int16_t)(h - HEIGHT_MIN_MM);
  if (remaining <= 0) {
    _buttons.consume(b);
    showNotice(UiText::Limit, dir == Motor::Direction::Up ? UiText::MaxHeight : UiText::MinHeight, nowMs);
    return;
  }
  _holdButton = b;
  _dir = dir;
  beginMove(nowMs);
  _state = State::Manual;
}

// ---------------------------------------------------------------------------------------------
//  Manual movement: runs while the button is held, slows down before the travel limit
// ---------------------------------------------------------------------------------------------
void DeskController::updateManual(uint32_t nowMs) {
  Button opposite = (_holdButton == Button::Up) ? Button::Down : Button::Up;
  if (_buttons.isActive(opposite)) {                                           // both buttons: stop and ignore both
    _motor.stop();
    _buttons.consume(Button::Up);
    _buttons.consume(Button::Down);
    _state = State::Idle;
    _stateSince = nowMs;
    return;
  }
  if (!_buttons.isActive(_holdButton)) {
    _motor.stop();
    _state = State::Idle;
    _stateSince = nowMs;
    return;
  }
  if (kTofBypass) {                                                            // bench test: only the move timeout remains
    if ((uint32_t)(nowMs - _moveStart) > MOVE_TIMEOUT_MS) {
      enterFault(FaultCode::Timeout, nowMs);
      return;
    }
    _motor.run(_dir, kMaxPermille);
    return;
  }
  if (!_tof.hasFreshHeight(nowMs)) {
    enterFault(FaultCode::Sensor, nowMs);
    return;
  }

  int16_t h = _tof.heightMm();
  int16_t remaining = (_dir == Motor::Direction::Up) ? (int16_t)(HEIGHT_MAX_MM - h) : (int16_t)(h - HEIGHT_MIN_MM);
  if (remaining <= 0) {                                                        // travel limit: release the button to go the other way
    _motor.stop();
    _buttons.consume(_holdButton);
    _state = State::Idle;
    _stateSince = nowMs;
    return;
  }
  if (!monitorMove(h, nowMs)) return;
  _motor.run(_dir, approachSpeedPermille(remaining));
}

// ---------------------------------------------------------------------------------------------
//  Automatic movement to a preset
// ---------------------------------------------------------------------------------------------
void DeskController::startGoto(uint8_t presetIndex, uint32_t nowMs) {
  if (kTofBypass) {
    showNotice(UiText::Bypass, UiText::PresetsOff, nowMs);
    return;
  }
  if (!_presets.isSet(presetIndex)) {
    showNotice(UiText::NoPreset, UiText::HoldToSave, nowMs);
    return;
  }
  if (!currentMonitorReady(nowMs)) return;
  if (!_tof.hasFreshHeight(nowMs)) {
    showNotice(UiText::NoSensor, UiText::HeightUnknown, nowMs);
    return;
  }
  int16_t target = clampHeight((int16_t)_presets.get(presetIndex));
  int16_t h = _tof.heightMm();
  int16_t delta = (int16_t)(target - h);
  if (delta >= -TARGET_TOLERANCE_MM && delta <= TARGET_TOLERANCE_MM) return;   // already there
  _target = target;
  _dir = delta > 0 ? Motor::Direction::Up : Motor::Direction::Down;
  beginMove(nowMs);
  _state = State::GotoPreset;
}

void DeskController::updateGoto(uint32_t nowMs) {
  if (!_tof.hasFreshHeight(nowMs)) {
    enterFault(FaultCode::Sensor, nowMs);
    return;
  }
  int16_t h = _tof.heightMm();
  int16_t remaining = (_dir == Motor::Direction::Up) ? (int16_t)(_target - h) : (int16_t)(h - _target);
  if (remaining <= TARGET_TOLERANCE_MM) {                                      // arrived (or slightly past): soft stop
    _motor.stop();
    _state = State::Idle;
    _stateSince = nowMs;
    return;
  }
  if (!monitorMove(h, nowMs)) return;
  _motor.run(_dir, approachSpeedPermille(remaining));
}

// Shared safety net for both kinds of movement. Returns false when a fault was raised.
bool DeskController::monitorMove(int16_t heightMm, uint32_t nowMs) {
  uint32_t dt = nowMs - _lastMonitor;
  _lastMonitor = nowMs;
  if (dt > 500) dt = 500;

  if ((uint32_t)(nowMs - _moveStart) > MOVE_TIMEOUT_MS) {
    enterFault(FaultCode::Timeout, nowMs);
    return false;
  }

  // The desk went the wrong way: DIR level or height sign misconfigured (or ToF not looking where expected).
  int16_t travel = (int16_t)(heightMm - _moveStartHeight);
  int16_t signedTravel = (_dir == Motor::Direction::Up) ? travel : (int16_t)-travel;
  if (signedTravel <= -WRONG_DIRECTION_TRAVEL_MM) {
    enterFault(FaultCode::WrongWay, nowMs);
    return false;
  }

  // Stall: the duty is high enough to expect motion but the height does not change.
  if (_motor.dutyPermille() >= kMinPermille) {
    int16_t d = (int16_t)(heightMm - _progressHeight);
    if (d < 0) d = (int16_t)-d;
    if (d >= STALL_MIN_PROGRESS_MM) {
      _progressHeight = heightMm;
      _stallMs = 0;
    } else {
      _stallMs = (uint16_t)(_stallMs + dt);
      if (_stallMs >= STALL_TIMEOUT_MS) {
        enterFault(FaultCode::Stall, nowMs);
        return false;
      }
    }
  }
  return true;
}

// ---------------------------------------------------------------------------------------------
//  Saving a preset (long press)
// ---------------------------------------------------------------------------------------------
void DeskController::startSaving(Button b, uint32_t nowMs) {
  if (kTofBypass) {
    _buttons.consume(b);
    showNotice(UiText::Bypass, UiText::PresetsOff, nowMs);
    return;
  }
  if (!_tof.hasFreshHeight(nowMs)) {
    _buttons.consume(b);
    showNotice(UiText::NoSensor, UiText::HeightUnknown, nowMs);
    return;
  }
  _saveButton = b;
  _presetIndex = presetIndexOf(b);
  _state = State::Saving;
  _stateSince = nowMs;
  _overlay = Overlay::None;
}

void DeskController::updateSaving(uint32_t nowMs) {
  (void)nowMs;   // nothing to do: the gauge follows the button hold time (see refreshUi), events end the state
}

void DeskController::finishSaving(uint32_t nowMs) {
  _state = State::Idle;
  _stateSince = nowMs;
  if (!_tof.hasFreshHeight(nowMs)) {
    showNotice(UiText::NoSensor, UiText::NotSaved, nowMs);
    return;
  }
  int16_t h = _tof.heightMm();
  if (h < 0) h = 0;
  if (_presets.save(_presetIndex, (uint16_t)h)) {
    _overlay = Overlay::Success;
    _overlayUntil = nowMs + OVERLAY_SUCCESS_MS;
    _successPreset = (uint8_t)(_presetIndex + 1);
    _successHeight = h;
    DBG(F("preset "));
    DBG(_successPreset);
    DBG(F(" saved: "));
    DBGLN(h);
  } else {
    showNotice(UiText::SaveError, UiText::Eeprom, nowMs);
  }
}

// ---------------------------------------------------------------------------------------------
//  Faults
// ---------------------------------------------------------------------------------------------
void DeskController::enterFault(FaultCode code, uint32_t nowMs) {
  _motor.emergencyStop();
  _state = State::Fault;
  _fault = code;
  _stateSince = nowMs;
  _overlay = Overlay::None;
  DBG(F("FAULT "));
  DBGLN((uint8_t)code);
}

// A lost link is not shown: without its cord the HMI board has no power, so there is nobody to show it to,
// and once the cord is back a message would only state the obvious. The motor is stopped all the same.
void DeskController::updateFault(uint32_t nowMs) {
  bool cleared = true;
  if (_fault == FaultCode::Link) cleared = !_buttons.ioError() && _link.connectedForMs(nowMs) >= HMI_LINK_SETTLE_MS;
  else if (_fault == FaultCode::Sensor) cleared = _tof.hasFreshHeight(nowMs);
  else if (_fault == FaultCode::CurrentSense) cleared = _current.present();

  bool shownLongEnough = _fault == FaultCode::Link || (uint32_t)(nowMs - _stateSince) >= FAULT_MIN_DISPLAY_MS;
  if (cleared && shownLongEnough && !_buttons.anyPressed()) {
    _buttons.reset();
    _fault = FaultCode::None;
    _state = State::Idle;
    _stateSince = nowMs;
  }
}

void DeskController::showNotice(UiText title, UiText subtitle, uint32_t nowMs) {
  _overlay = Overlay::Notice;
  _overlayUntil = nowMs + OVERLAY_NOTICE_MS;
  _noticeTitle = title;
  _noticeSubtitle = subtitle;
}

// ---------------------------------------------------------------------------------------------
//  Screen content
// ---------------------------------------------------------------------------------------------
void DeskController::refreshUi(uint32_t nowMs) {
  _ui.heightMm = _tof.heightMm();
  _ui.heightValid = _tof.hasFreshHeight(nowMs);
  _ui.arrow = UiModel::Arrow::None;

  // A lost link shows what Idle would show (see updateFault()): it only matters once the link is back.
  State shown = (_state == State::Fault && _fault == FaultCode::Link) ? State::Idle : _state;
  switch (shown) {
    case State::Idle:
      if ((int32_t)(nowMs - _bootUntil) < 0 && _overlay == Overlay::None) {
        _ui.screen = UiModel::Screen::Boot;
        _ui.title = UiText::Desktopia;
        _ui.subtitle = kTofBypass ? UiText::SensorBypass : UiText::Starting;
      } else if (_overlay == Overlay::Notice) {
        _ui.screen = UiModel::Screen::Notice;
        _ui.title = _noticeTitle;
        _ui.subtitle = _noticeSubtitle;
      } else if (_overlay == Overlay::Success) {
        _ui.screen = UiModel::Screen::SaveSuccess;
        _ui.presetNumber = _successPreset;
        _ui.savedHeightMm = _successHeight;
      } else {
        _ui.screen = UiModel::Screen::Main;
      }
      break;

    case State::Manual:
    case State::GotoPreset:
      _ui.screen = UiModel::Screen::Main;
      _ui.arrow = (_dir == Motor::Direction::Up) ? UiModel::Arrow::Up : UiModel::Arrow::Down;
      break;

    case State::Saving: {
      _ui.screen = UiModel::Screen::SaveGauge;
      _ui.presetNumber = (uint8_t)(_presetIndex + 1);
      uint16_t held = _buttons.heldMs(_saveButton, nowMs);
      uint32_t pct = 0;
      if (held > PRESET_SHORT_PRESS_MAX_MS) {
        pct = (uint32_t)(held - PRESET_SHORT_PRESS_MAX_MS) * 100UL / (PRESET_LONG_PRESS_MS - PRESET_SHORT_PRESS_MAX_MS);
      }
      _ui.gaugePercent = pct > 100 ? 100 : (uint8_t)pct;
      break;
    }

    case State::Fault:
      _ui.screen = UiModel::Screen::Fault;
      switch (_fault) {
        case FaultCode::Sensor:
          _ui.title = UiText::SensorLost;
          _ui.subtitle = UiText::TofNotAnswering;
          break;
        case FaultCode::WrongWay:
          _ui.title = UiText::WrongWay;
          _ui.subtitle = UiText::CheckDirSign;
          break;
        case FaultCode::Stall:
          _ui.title = UiText::Stalled;
          _ui.subtitle = UiText::DeskNotMoving;
          break;
        case FaultCode::Timeout:
          _ui.title = UiText::Timeout;
          _ui.subtitle = UiText::MoveTooLong;
          break;
        case FaultCode::Watchdog:
          _ui.title = UiText::Watchdog;
          _ui.subtitle = UiText::LoopStuck;
          break;
        case FaultCode::Overcurrent:
          _ui.title = UiText::Overcurrent;
          _ui.subtitle = UiText::MotorCurrentHigh;
          break;
        case FaultCode::CurrentSense:
          _ui.title = UiText::NoCurrentSense;
          _ui.subtitle = UiText::Ina226NotAnswering;
          break;

        case FaultCode::Link:
        case FaultCode::None:
          break;
      }

      break;
  }
}
