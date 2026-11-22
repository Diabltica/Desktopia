#pragma once
/*
 * Desktopia motherboard – pin map extracted from desktopia.kicad_sch (RJ11 / RS-485 HMI version)
 * ---------------------------------------------------------------------------
 * MCU: RP2040 (U4, QFN-56), 12 MHz crystal, 3.3 V I/O (U9). Numbers below are GPIO numbers (GPn).
 * (QFN pin / schematic net in the comments). No pin is 5 V tolerant: every input on this board is at 3.3 V.
 */
#include <Arduino.h>

// The PC simulation (firmware/sim) runs both boards on one pin array: it moves this board's pins out of the way.
#ifndef BOARD_PIN_OFFSET
#define BOARD_PIN_OFFSET 0
#endif

namespace pins {

// --- Motor command: on-board H-bridge (through JP2-JP5) and connector J6 (1 = Mdir, 2 = Mpwm, 3 = GND),
//     10k pull-downs R24 / R26
constexpr uint8_t MOTOR_PWM = 8 + BOARD_PIN_OFFSET;     // GP8  – pin 11 – net Mpwm – PWM slice 4, channel A
constexpr uint8_t MOTOR_DIR = 9 + BOARD_PIN_OFFSET;     // GP9  – pin 12 – net Mdir

// --- Motor driver supply: HIGH = Q5 (P-MOSFET high-side switch) feeds J5 and the bridge. Pull-down R22.
constexpr uint8_t MOTOR_POWER = 12 + BOARD_PIN_OFFSET;  // GP12 – pin 15 – net MOTOR_PWR

// --- DRV8701E (U7), not driven by this firmware yet: nSLEEP stays low (its pull-down), the bridge sleeps
constexpr uint8_t BRIDGE_NSLEEP = 10 + BOARD_PIN_OFFSET;   // GP10 – pin 13 – net nSLEEP
constexpr uint8_t BRIDGE_NFAULT = 11 + BOARD_PIN_OFFSET;   // GP11 – pin 14 – net nFAULT (open drain)

// --- INA226 alert: open drain, active low, 4.7k pull-up R25 to +3V3, 100 pF filter C23
constexpr uint8_t I_ALERT = 7 + BOARD_PIN_OFFSET;       // GP7  – pin 9  – net I_ALERT

// --- HMI board link: RS-485 transceiver U5 (SP3485EN, 3.3 V), RJ11 jack J4. UART0 → Serial1
constexpr uint8_t RS485_RX = 17 + BOARD_PIN_OFFSET;     // GP17 – pin 28 – U5 RO (UART0 RX)
constexpr uint8_t RS485_DE = 18 + BOARD_PIN_OFFSET;     // GP18 – pin 29 – U5 DE and /RE, tied: HIGH = drive the bus
constexpr uint8_t RS485_TX = 28 + BOARD_PIN_OFFSET;     // GP28 – pin 40 – U5 DI (UART0 TX)

// --- I2C bus: INA226 U6 (0x40) on the 3.3 V side (R1 / R5 pull-ups), VL53L1CB U3 (0x29) on the 2.8 V side of the
//     BSS138 level shifters Q1 / Q2
constexpr uint8_t I2C_SDA = 4 + BOARD_PIN_OFFSET;       // GP4  – pin 6  – net SDA (I2C0)
constexpr uint8_t I2C_SCL = 5 + BOARD_PIN_OFFSET;       // GP5  – pin 7  – net SCL (I2C0)

// --- Not used by this firmware
//   GP6 (pin 8) = TOF_INT: VL53L1CB GPIO1 (data ready) through the level shifter Q3, R10 10k pull-up to +3V3
//   GP0-GP3, GP13-GP16, GP19-GP27, GP29: not connected
//   SWD: TP4 SWCLK, TP7 SWDIO. BOOT1 pulls QSPI_SS low (USB bootloader), RST1 pulls RUN low.

}  // namespace pins
