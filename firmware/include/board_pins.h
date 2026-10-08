#pragma once
/*
 * Desktopia motherboard – pin map extracted from desktopia.kicad_sch
 * ---------------------------------------------------------------------------
 * MCU: ATmega32U4 (TQFP-44), 16 MHz crystal, +5 V.
 * Numbers below are Arduino pin numbers of the PlatformIO "leonardo" variant.
 * (port / TQFP pin / schematic net in the comments)
 */
#include <Arduino.h>

namespace pins {

// --- Motor command, connector J5 "MotorCommandCon" (1 = Mdir, 2 = Mpwm, 3 = GND)
constexpr uint8_t MOTOR_DIR = 5;    // PC6  – TQFP 31 – net Mdir
constexpr uint8_t MOTOR_PWM = 10;   // PB6  – TQFP 30 – net Mpwm – OC1B (Timer1 hardware PWM)

// --- Round LCD on J1 (7 pins, SPI write-only) – reaches the HMI through the FFC cable
constexpr uint8_t LCD_CS  = 17;     // PB0  – TQFP  8 – net CS_MBside   (also the core's RX LED, see README)
constexpr uint8_t LCD_DC  = 8;      // PB4  – TQFP 28 – net DC_MBside
constexpr uint8_t LCD_RST = 9;      // PB5  – TQFP 29 – net RST_MBside
//   SCK  = PB1 (D15) and MOSI = PB2 (D16): hardware SPI, nothing to configure here

// --- I2C bus: TCA9554 I/O expander (0x20) + VL53L1CB ToF (0x29) behind the TXS0104E
//   SDA = PD1 (D2), SCL = PD0 (D3): hardware TWI via Wire

// --- Spare signals that are wired but not needed by this firmware
constexpr uint8_t HMI_INT   = 7;    // PE6 – TQFP  1 – TCA9554 /INT, open-drain, NO external pull-up
constexpr uint8_t TOF_GPIO1 = 0;    // PD2 – TQFP 20 – VL53L1CB GPIO1 (through level shifter)
//   PF0 (A5) = Imotor: current sensor U7 is not populated → ignored by design

}  // namespace pins
