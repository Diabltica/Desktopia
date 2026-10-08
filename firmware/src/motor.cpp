#include "motor.h"
#include "board_pins.h"
#include "config.h"
#include <avr/io.h>
#include <util/atomic.h>

static_assert(MOTOR_DIR_UP_LEVEL != MOTOR_DIR_DOWN_LEVEL, "UP and DOWN levels must differ");
static_assert(MOTOR_PWM_MAX_PERCENT > 0 && MOTOR_PWM_MAX_PERCENT <= 100, "MOTOR_PWM_MAX_PERCENT out of range");
static_assert(MOTOR_PWM_MIN_PERCENT >= 0 && MOTOR_PWM_MIN_PERCENT < MOTOR_PWM_MAX_PERCENT, "MOTOR_PWM_MIN_PERCENT out of range");
static_assert(MOTOR_SOFT_START_MS > 0 && MOTOR_SOFT_STOP_MS > 0, "ramp times must be positive");
static_assert(F_CPU / MOTOR_PWM_FREQ_HZ >= 100 && F_CPU / MOTOR_PWM_FREQ_HZ <= 65536UL, "MOTOR_PWM_FREQ_HZ out of range (245 Hz .. 160 kHz)");

namespace {
volatile bool g_failsafe = false;

constexpr uint32_t kMaxPermille = (uint32_t)MOTOR_PWM_MAX_PERCENT * 10UL;
constexpr uint32_t kMaxQ10 = kMaxPermille << 10;
constexpr uint32_t kRateUp = kMaxQ10 / MOTOR_SOFT_START_MS;      // Q10-permille per ms
constexpr uint32_t kRateDown = kMaxQ10 / MOTOR_SOFT_STOP_MS;
}  // namespace

void motorFailsafeCut() {
  TCCR1A &= (uint8_t) ~_BV(COM1B1);      // disconnect OC1B from the pin
  PORTB &= (uint8_t) ~_BV(PORTB6);       // and force it low
  g_failsafe = true;
}

void Motor::begin() {
  pinMode(pins::MOTOR_PWM, OUTPUT);
  digitalWrite(pins::MOTOR_PWM, LOW);
  pinMode(pins::MOTOR_DIR, OUTPUT);
  setDirectionPin(Direction::Down);

  _top = (uint16_t)(F_CPU / MOTOR_PWM_FREQ_HZ - 1);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Timer1, mode 14 (fast PWM, TOP = ICR1), prescaler 1. OC1B stays disconnected while the duty is 0.
    TCCR1A = _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
    ICR1 = _top;
    OCR1B = 0;
  }
  _dutyQ10 = 0;
  _hasRequest = false;
  _wasMoving = false;
  _lastUpdate = millis();
  _stoppedAt = _lastUpdate - MOTOR_REVERSE_PAUSE_MS;
}

void Motor::setDirectionPin(Direction d) {
  digitalWrite(pins::MOTOR_DIR, d == Direction::Up ? MOTOR_DIR_UP_LEVEL : MOTOR_DIR_DOWN_LEVEL);
  _appliedDir = d;
}

void Motor::applyDuty() {
  uint16_t counts = 0;
  if (_dutyQ10 != 0) {
    counts = (uint16_t)(((uint32_t)(_dutyQ10 >> 4) * _top) / 64000UL);   // (permille x 64) x TOP / (1000 x 64)
    if (counts > _top) counts = _top;
  }
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (counts == 0) {
      // A fast-PWM compare value of 0 still emits a 1-tick pulse: disconnect the pin instead.
      TCCR1A &= (uint8_t) ~_BV(COM1B1);
      PORTB &= (uint8_t) ~_BV(PORTB6);
      OCR1B = 0;
    } else {
      OCR1B = counts;
      TCCR1A |= _BV(COM1B1);                       // non-inverting output on OC1B
    }
  }
}

void Motor::run(Direction dir, uint16_t targetPermille) {
  if (targetPermille > kMaxPermille) targetPermille = (uint16_t)kMaxPermille;
  _reqDir = dir;
  _targetPermille = targetPermille;
  _hasRequest = targetPermille > 0;
}

void Motor::stop() { _hasRequest = false; }

void Motor::emergencyStop() {
  _hasRequest = false;
  _dutyQ10 = 0;
  _wasMoving = false;
  _stoppedAt = millis();
  applyDuty();
}

bool Motor::consumeFailsafeFlag() {
  bool f;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    f = g_failsafe;
    g_failsafe = false;
  }
  if (f) {
    _hasRequest = false;
    _dutyQ10 = 0;
    _wasMoving = false;
    _stoppedAt = millis();
    applyDuty();
  }
  return f;
}

void Motor::update(uint32_t nowMs) {
  uint32_t dt = nowMs - _lastUpdate;
  if (dt == 0) return;
  _lastUpdate = nowMs;
  if (dt > 100) dt = 100;

  uint32_t targetQ = 0;
  if (_hasRequest) {
    if (_reqDir == _appliedDir) {
      targetQ = (uint32_t)_targetPermille << 10;
    } else if (_dutyQ10 == 0 && (uint32_t)(nowMs - _stoppedAt) >= MOTOR_REVERSE_PAUSE_MS) {
      setDirectionPin(_reqDir);                    // safe: the output is at 0 %
      targetQ = (uint32_t)_targetPermille << 10;
    }                                              // else: ramp down first, the direction flips later
  }

  if (_dutyQ10 < targetQ) {                        // soft start / speed up
    uint32_t step = kRateUp * dt;
    _dutyQ10 = (targetQ - _dutyQ10 > step) ? _dutyQ10 + step : targetQ;
  } else if (_dutyQ10 > targetQ) {                 // soft stop / slow down
    uint32_t step = kRateDown * dt;
    _dutyQ10 = (_dutyQ10 - targetQ > step) ? _dutyQ10 - step : targetQ;
  }

  if (_dutyQ10 != 0) {
    _wasMoving = true;
  } else if (_wasMoving) {
    _wasMoving = false;
    _stoppedAt = nowMs;
  }
  applyDuty();
}
