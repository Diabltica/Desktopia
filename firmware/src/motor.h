#pragma once
#include <Arduino.h>

// DC motor driven through a DIR pin and a hardware PWM pin (Timer1 / OC1B).
//  - duty is capped at MOTOR_PWM_MAX_PERCENT, whatever the caller asks;
//  - every start is a soft start (linear ramp), every slow-down / stop is a soft stop;
//  - the DIR pin only changes while the duty is 0 and after MOTOR_REVERSE_PAUSE_MS.
class Motor {
 public:
  enum class Direction : uint8_t { Up, Down };

  void begin();                                       // outputs low, Timer1 configured – call first thing in setup()
  void update(uint32_t nowMs);                        // integrates the ramp: call every loop

  void run(Direction dir, uint16_t targetPermille);   // 0..1000, clamped to the maximum; 0 = soft stop
  void stop();                                        // soft stop
  void emergencyStop();                               // duty 0 right now, no ramp

  bool isRunning() const { return _dutyQ10 != 0 || _hasRequest; }
  bool isStopped() const { return !isRunning(); }
  Direction direction() const { return _appliedDir; }
  uint16_t dutyPermille() const { return (uint16_t)(_dutyQ10 >> 10); }

  // True once if the hardware watchdog interrupt had to cut the PWM behind the loop's back.
  bool consumeFailsafeFlag();

 private:
  void applyDuty();
  void setDirectionPin(Direction d);

  uint32_t _dutyQ10 = 0;        // current duty in permille x 1024
  uint16_t _targetPermille = 0;
  Direction _reqDir = Direction::Down;
  Direction _appliedDir = Direction::Down;
  bool _hasRequest = false;
  bool _wasMoving = false;
  uint32_t _lastUpdate = 0;
  uint32_t _stoppedAt = 0;
  uint16_t _top = 0;            // Timer1 TOP for MOTOR_PWM_FREQ_HZ
};

// Cuts the PWM output using register writes only: safe to call from an interrupt.
void motorFailsafeCut();
