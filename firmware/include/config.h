#pragma once
/*
 * Desktopia – every tunable of the firmware lives in this file.
 * Units are part of the names (_MS, _MM, _PERCENT, _HZ).
 * Any value can also be overridden from platformio.ini with -DNAME=value.
 */
#include <Arduino.h>

#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL 0               // 1 = log on the USB serial port (TX only, see README)
#endif

/* ---------------------------------------------------------------------------
 *  Motor (DIR + PWM on J5)
 * ------------------------------------------------------------------------- */
// Level written on the DIR pin to move the desk UP / DOWN.
// Swap the two lines if the motor driver turns the other way.
#ifndef MOTOR_DIR_UP_LEVEL
#define MOTOR_DIR_UP_LEVEL         HIGH   // 1 → desk goes up
#endif
#ifndef MOTOR_DIR_DOWN_LEVEL
#define MOTOR_DIR_DOWN_LEVEL       LOW    // 0 → desk goes down
#endif

#ifndef MOTOR_PWM_MAX_PERCENT
#define MOTOR_PWM_MAX_PERCENT      60     // hard ceiling, never exceeded
#endif
#ifndef MOTOR_PWM_MIN_PERCENT
#define MOTOR_PWM_MIN_PERCENT      10     // lowest duty that still moves the desk (final approach) – tune it
#endif
#ifndef MOTOR_PWM_FREQ_HZ
#define MOTOR_PWM_FREQ_HZ          4000UL   // Timer1 fast PWM. MD10C R3: 20 kHz max; its switching losses grow with the
                                            // frequency → lower = cooler driver but audible whine (try 4000–10000)
#endif
#ifndef MOTOR_SOFT_START_MS
#define MOTOR_SOFT_START_MS        1500   // 0 → max ramp, applied every time the motor starts
#endif
#ifndef MOTOR_SOFT_STOP_MS
#define MOTOR_SOFT_STOP_MS         300    // max → 0 ramp, also limits how fast the speed may drop
#endif
#ifndef MOTOR_REVERSE_PAUSE_MS
#define MOTOR_REVERSE_PAUSE_MS     400    // dwell at 0 % before the DIR pin is toggled
#endif

/* ---------------------------------------------------------------------------
 *  Height measurement (VL53L1CB through the ST/STM32duino driver)
 *  height_mm = TOF_HEIGHT_OFFSET_MM + TOF_HEIGHT_SIGN * distance_mm
 * ------------------------------------------------------------------------- */
#ifndef TOF_I2C_ADDR_8BIT
#define TOF_I2C_ADDR_8BIT          0x52   // ST drivers use the 8-bit form (0x29 on 7 bits)
#endif
#ifndef TOF_HEIGHT_SIGN
#define TOF_HEIGHT_SIGN            (+1)   // +1: reading grows when the desk goes up; -1: it shrinks
#endif
#ifndef TOF_HEIGHT_OFFSET_MM
#define TOF_HEIGHT_OFFSET_MM       0
#endif
#ifndef TOF_TIMING_BUDGET_MS
#define TOF_TIMING_BUDGET_MS       33     // long mode: 20 / 33 / 50 / 100 / 200 / 500
#endif
#ifndef TOF_FILTER_WINDOW
#define TOF_FILTER_WINDOW          5      // median filter length: odd number, 1..9
#endif
#ifndef TOF_STALE_TIMEOUT_MS
#define TOF_STALE_TIMEOUT_MS       400    // no valid sample for this long = height unusable
#endif
#ifndef TOF_COMM_FAIL_LIMIT
#define TOF_COMM_FAIL_LIMIT        4      // consecutive I2C failures before the sensor is declared offline
#endif
#ifndef TOF_RETRY_PERIOD_MS
#define TOF_RETRY_PERIOD_MS        1000   // re-init attempts while offline (only when the motor is stopped)
#endif
// BENCH TEST ONLY – motor decoupled from the desk. 1 = the height is ignored for motor control:
// Up/Down run the motor at MOTOR_PWM_MAX_PERCENT as long as they are held, with no end stops,
// no sensor / stall / wrong-way checks (MOVE_TIMEOUT_MS still applies). Presets are disabled.
// The ToF keeps running and its reading is still displayed when it is connected.
#ifndef TOF_BYPASS
#define TOF_BYPASS                 0
#endif

/* ---------------------------------------------------------------------------
 *  Travel limits and motion profile (all heights in mm, as read by the ToF)
 * ------------------------------------------------------------------------- */
#ifndef HEIGHT_MIN_MM
#define HEIGHT_MIN_MM              640    // set to your real mechanical range
#endif
#ifndef HEIGHT_MAX_MM
#define HEIGHT_MAX_MM              1200
#endif
#ifndef APPROACH_DISTANCE_MM
#define APPROACH_DISTANCE_MM       80     // below this remaining distance the speed ramps down to MIN
#endif
#ifndef TARGET_TOLERANCE_MM
#define TARGET_TOLERANCE_MM        2      // a move ends when the remaining distance is <= this
#endif

/* ---------------------------------------------------------------------------
 *  Safety nets (no current sensing on this board, so everything relies on the ToF)
 * ------------------------------------------------------------------------- */
#ifndef MOVE_TIMEOUT_MS
#define MOVE_TIMEOUT_MS            60000UL  // maximum duration of one movement
#endif
#ifndef STALL_TIMEOUT_MS
#define STALL_TIMEOUT_MS           1500     // motor above MIN duty and no progress for this long = stall
#endif
#ifndef STALL_MIN_PROGRESS_MM
#define STALL_MIN_PROGRESS_MM      3
#endif
#ifndef WRONG_DIRECTION_TRAVEL_MM
#define WRONG_DIRECTION_TRAVEL_MM  15       // moved this far the wrong way = DIR / SIGN misconfigured
#endif
#ifndef WATCHDOG_TIMEOUT
#define WATCHDOG_TIMEOUT           WDTO_500MS  // hardware watchdog (interrupt cuts the PWM, reset follows)
#endif

/* ---------------------------------------------------------------------------
 *  Buttons on the TCA9554 (SW1..SW5 = P0..P4, active HIGH: switch to +5VA, 10k pull-downs)
 * ------------------------------------------------------------------------- */
#ifndef IOEXP_I2C_ADDR
#define IOEXP_I2C_ADDR             0x20   // A0 = A1 = A2 = GND
#endif
#ifndef IOEXP_DIR_MASK
#define IOEXP_DIR_MASK             0x1F   // P0..P4 = buttons (inputs); P5..P7 are unconnected → driven low as outputs
#endif
#ifndef BTN_ACTIVE_LEVEL
#define BTN_ACTIVE_LEVEL           HIGH
#endif
#ifndef BTN_BIT_UP
#define BTN_BIT_UP                 0      // SW1
#endif
#ifndef BTN_BIT_DOWN
#define BTN_BIT_DOWN               1      // SW2
#endif
#ifndef BTN_BIT_PRESET1
#define BTN_BIT_PRESET1            2      // SW3
#endif
#ifndef BTN_BIT_PRESET2
#define BTN_BIT_PRESET2            3      // SW4
#endif
#ifndef BTN_BIT_PRESET3
#define BTN_BIT_PRESET3            4      // SW5
#endif
#ifndef BTN_SAMPLE_PERIOD_MS
#define BTN_SAMPLE_PERIOD_MS       5
#endif
#ifndef BTN_DEBOUNCE_SAMPLES
#define BTN_DEBOUNCE_SAMPLES       4      // 4 x 5 ms = 20 ms
#endif
#ifndef BTN_IO_FAIL_LIMIT
#define BTN_IO_FAIL_LIMIT          3      // consecutive failed expander reads before a fault is raised
#endif

#ifndef PRESET_SHORT_PRESS_MAX_MS
#define PRESET_SHORT_PRESS_MAX_MS  400    // released before this: go to the preset
#endif
#ifndef PRESET_LONG_PRESS_MS
#define PRESET_LONG_PRESS_MS       2000   // held this long: save the current height (gauge runs 400 → 2000 ms)
#endif
#ifndef PRESET_COUNT
#define PRESET_COUNT               3
#endif
#ifndef PRESET_EEPROM_ADDR
#define PRESET_EEPROM_ADDR         0
#endif

/* ---------------------------------------------------------------------------
 *  I2C bus
 * ------------------------------------------------------------------------- */
#ifndef I2C_CLOCK_HZ
#define I2C_CLOCK_HZ               400000UL
#endif
#ifndef I2C_TIMEOUT_US
#define I2C_TIMEOUT_US             25000UL  // Wire timeout: a stuck bus can never block the loop
#endif

/* ---------------------------------------------------------------------------
 *  Display (round 240x240 SPI LCD, GC9A01)
 * ------------------------------------------------------------------------- */
#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION           1        // 0..3 (1 = 90° clockwise)
#endif
#ifndef DISPLAY_SPI_HZ
#define DISPLAY_SPI_HZ             8000000UL
#endif
#ifndef DISPLAY_MIN_PERIOD_MS
#define DISPLAY_MIN_PERIOD_MS      25       // refresh limiter
#endif
#ifndef DISPLAY_DEADBAND_MM
#define DISPLAY_DEADBAND_MM        2        // ignore +-2 mm jitter on the number while the desk is still
#endif
#ifndef OVERLAY_NOTICE_MS
#define OVERLAY_NOTICE_MS          1500
#endif
#ifndef OVERLAY_SUCCESS_MS
#define OVERLAY_SUCCESS_MS         1800
#endif
#ifndef FAULT_MIN_DISPLAY_MS
#define FAULT_MIN_DISPLAY_MS       2500
#endif
