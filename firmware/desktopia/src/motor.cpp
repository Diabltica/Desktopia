#include "motor.h"
#include "board_pins.h"
#include "config.h"
#include <hardware/clocks.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/sync.h>

static_assert(MOTOR_DIR_UP_LEVEL != MOTOR_DIR_DOWN_LEVEL, "UP and DOWN levels must differ");
static_assert(MOTOR_PWM_MAX_PERCENT > 0 && MOTOR_PWM_MAX_PERCENT <= 100, "MOTOR_PWM_MAX_PERCENT out of range");
static_assert(MOTOR_PWM_MIN_PERCENT >= 0 && MOTOR_PWM_MIN_PERCENT < MOTOR_PWM_MAX_PERCENT, "MOTOR_PWM_MIN_PERCENT out of range");
static_assert(MOTOR_SOFT_START_MS > 0 && MOTOR_SOFT_STOP_MS > 0, "ramp times must be positive");
static_assert(MOTOR_PWM_FREQ_HZ >= 100UL && MOTOR_PWM_FREQ_HZ <= 100000UL, "MOTOR_PWM_FREQ_HZ out of range (100 Hz .. 100 kHz)");
static_assert(MOTOR_PWM_FREQ_HZ <= 20000UL, "MOTOR_PWM_FREQ_HZ above the 20 kHz rating of the MD10C R3 driver");
static_assert(MOTOR_POWER_SETTLE_MS >= 30, "Q5 needs ~15 ms to switch the driver supply on, the driver a few more");
static_assert(MOTOR_POWER_IDLE_OFF_MS >= 1000, "MOTOR_POWER_IDLE_OFF_MS too short: the supply would toggle between moves");

namespace {
volatile bool g_failsafe = false;
volatile bool g_cutLatched = false;   // motorHardCut() ran: no PWM, no supply until Motor aligns its state

constexpr uint32_t kMaxPermille = (uint32_t)MOTOR_PWM_MAX_PERCENT * 10UL;
constexpr uint32_t kMaxQ10 = kMaxPermille << 10;
constexpr uint32_t kRateUp = kMaxQ10 / MOTOR_SOFT_START_MS;      // Q10-permille per ms
constexpr uint32_t kRateDown = kMaxQ10 / MOTOR_SOFT_STOP_MS;

// The PWM pin stays on its PWM slice; its pad output is forced low (output override) whenever the duty is 0 or the
// motor is cut. One register write, effective at once, whatever the slice is doing (a new compare value would only
// load at the end of the PWM period).
inline void pwmPinForceLow() { gpio_set_outover(pins::MOTOR_PWM, GPIO_OVERRIDE_LOW); }
inline void pwmPinRelease() { gpio_set_outover(pins::MOTOR_PWM, GPIO_OVERRIDE_NORMAL); }
}  // namespace

void motorHardCut() {
  pwmPinForceLow();                      // PWM output low now
  gpio_put(pins::MOTOR_POWER, 0);        // then open Q5 (~20 ms): the driver is isolated
  g_cutLatched = true;                   // the loop may still be inside Motor::update(): it must not undo this
}

void motorFailsafeCut() {
  motorHardCut();
  g_failsafe = true;
}

void Motor::begin() {
  pinMode(pins::MOTOR_PWM, OUTPUT);
  digitalWrite(pins::MOTOR_PWM, LOW);
  pinMode(pins::MOTOR_DIR, OUTPUT);
  setDirectionPin(Direction::Down);
  pinMode(pins::MOTOR_POWER, OUTPUT);
  setPower(false, millis());

  // PWM slice of the pin: integer clock divider, then TOP so that the period is 1 / MOTOR_PWM_FREQ_HZ.
  // 125 MHz / 4 kHz = 31250 counts per period.
  uint32_t perPeriod = clock_get_hz(clk_sys) / MOTOR_PWM_FREQ_HZ;
  uint32_t div = (perPeriod + 65535UL) / 65536UL;
  if (div < 1) div = 1;
  _top = (uint16_t)(perPeriod / div - 1);
  uint slice = pwm_gpio_to_slice_num(pins::MOTOR_PWM);
  pwm_config cfg = pwm_get_default_config();
  pwm_config_set_clkdiv_int(&cfg, div);
  pwm_config_set_wrap(&cfg, _top);
  pwm_init(slice, &cfg, false);
  pwm_set_gpio_level(pins::MOTOR_PWM, 0);
  pwmPinForceLow();                                // the pin stays low until the first duty
  gpio_set_function(pins::MOTOR_PWM, GPIO_FUNC_PWM);
  pwm_set_enabled(slice, true);
  _dutyQ10 = 0;
  _hasRequest = false;
  _wasMoving = false;
  _lastUpdate = millis();
  _stoppedAt = _lastUpdate - MOTOR_REVERSE_PAUSE_MS;
  _lastActive = _lastUpdate;
}

void Motor::setDirectionPin(Direction d) {
  digitalWrite(pins::MOTOR_DIR, d == Direction::Up ? MOTOR_DIR_UP_LEVEL : MOTOR_DIR_DOWN_LEVEL);
  _appliedDir = d;
}

void Motor::setPower(bool on, uint32_t nowMs) {
  uint32_t irq = save_and_disable_interrupts();
  if (g_cutLatched) on = false;                    // checked and written with interrupts off: no race with the cut
  gpio_put(pins::MOTOR_POWER, on);
  restore_interrupts(irq);
  if (on && !_powered) _poweredAt = nowMs;
  _powered = on;
}

void Motor::applyDuty() {
  uint16_t counts = 0;
  if (_dutyQ10 != 0) {
    counts = (uint16_t)(((uint64_t)(_dutyQ10 >> 4) * (_top + 1UL)) / 64000UL);   // (permille x 64) x period / (1000 x 64)
    if (counts > _top) counts = _top;
  }
  uint32_t irq = save_and_disable_interrupts();
  if (counts == 0 || g_cutLatched) {
    pwmPinForceLow();                              // the pin is low at once, not at the end of the period
    pwm_set_gpio_level(pins::MOTOR_PWM, 0);
  } else {
    pwm_set_gpio_level(pins::MOTOR_PWM, counts);   // high while the counter is below the level
    pwmPinRelease();
  }
  restore_interrupts(irq);
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

void Motor::cutPower() {
  emergencyStop();                                 // PWM low first: the bridge stops drawing from the supply
  setPower(false, millis());
  g_cutLatched = false;                            // state aligned with the hardware: a new request may start again
}

bool Motor::consumeFailsafeFlag() {
  uint32_t irq = save_and_disable_interrupts();
  bool f = g_failsafe;
  g_failsafe = false;
  restore_interrupts(irq);
  if (f) cutPower();                               // the interrupt already did it in hardware: align the state
  return f;
}

void Motor::update(uint32_t nowMs) {
  if (g_cutLatched) cutPower();                    // an interrupt cut the motor: align the state, wait for a new request
  uint32_t dt = nowMs - _lastUpdate;
  if (dt == 0) return;
  _lastUpdate = nowMs;
  if (dt > 100) dt = 100;

  // Driver supply: on with the first request, PWM only once it has settled.
  if (_hasRequest || _dutyQ10 != 0) _lastActive = nowMs;
  if (_hasRequest && !_powered) setPower(true, nowMs);
  bool supplyReady = _powered && (uint32_t)(nowMs - _poweredAt) >= MOTOR_POWER_SETTLE_MS;

  uint32_t targetQ = 0;
  if (_hasRequest && supplyReady) {
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

  // Driver supply off after a long idle time: only with the PWM at 0, so Q5 never opens under load.
  if (_powered && _dutyQ10 == 0 && !_hasRequest && (uint32_t)(nowMs - _lastActive) >= MOTOR_POWER_IDLE_OFF_MS) {
    setPower(false, nowMs);
  }
}
