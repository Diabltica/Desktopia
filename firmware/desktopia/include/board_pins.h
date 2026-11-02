#pragma once
/*
 * Desktopia motherboard – pin map extracted from desktopia.kicad_sch (RJ11 / RS-485 HMI version)
 * ---------------------------------------------------------------------------
 * MCU: ATmega32U4 (TQFP-44), 16 MHz crystal, +5 V.
 * Numbers below are Arduino pin numbers of the PlatformIO "leonardo" variant.
 * (port / TQFP pin / schematic net in the comments)
 */
#include <Arduino.h>

namespace pins {

// --- Motor command, connector J6 "MotorCommandCon" (1 = Mdir, 2 = Mpwm, 3 = GND), 10k pull-downs
constexpr uint8_t MOTOR_DIR = 5;    // PC6  – TQFP 31 – net Mdir
constexpr uint8_t MOTOR_PWM = 10;   // PB6  – TQFP 30 – net Mpwm – OC1B (Timer1 hardware PWM)

// --- Motor driver supply: HIGH = Q5 (P-MOSFET high-side switch) feeds J5. Pull-down R22: off during a reset.
constexpr uint8_t MOTOR_POWER = 6;  // PD7  – TQFP 27 – net MOTOR_PWR
constexpr uint8_t MOTOR_POWER_PORT_BIT = 7;   // PORTD bit, for the register-only cut in interrupts

// --- INA226 alert: open drain, active low, 4.7k pull-up R25, 100 pF filter C23
constexpr uint8_t I_ALERT = 7;      // PE6  – TQFP 1  – net I_ALERT – INT6

// --- HMI board link: RS-485 transceiver U5 (SP485EEN), RJ11 jack J4
//   RX = PD2 (D0) – TQFP 20 – U5 RO            TX = PD3 (D1) – TQFP 21 – U5 DI          → Serial1
constexpr uint8_t RS485_DE = 4;     // PD4  – TQFP 25 – U5 DE and /RE, tied: HIGH = drive the bus

// --- I2C bus: VL53L1CB ToF (0x29) behind the BSS138 level shifters Q1 / Q2 (2.8 V side),
//     INA226 motor current monitor U6 (0x40) on the 5 V side
//   SDA = PD1 (D2), SCL = PD0 (D3): hardware TWI via Wire

// --- Not used by this firmware
//   PB4 (D8) = TOF_INT: VL53L1CB GPIO1 (data ready) through level shifter Q3, 10k pull-up R10 (PCINT4)
//   PF0 (A5) is now free (it read the old analog current sensor)
//   PB1 / PB2 / PB3 (SCK / MOSI / MISO) only go to the ISP header J2

}  // namespace pins
