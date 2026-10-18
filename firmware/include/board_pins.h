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

// --- Motor command, connector J5 "MotorCommandCon" (1 = Mdir, 2 = Mpwm, 3 = GND)
constexpr uint8_t MOTOR_DIR = 5;    // PC6  – TQFP 31 – net Mdir
constexpr uint8_t MOTOR_PWM = 10;   // PB6  – TQFP 30 – net Mpwm – OC1B (Timer1 hardware PWM)

// --- HMI board link: RS-485 transceiver U8 (THVD1450), RJ11 jack J6
//   RX = PD2 (D0) – TQFP 20 – net RS485_RO      TX = PD3 (D1) – TQFP 21 – net RS485_DI   → Serial1
constexpr uint8_t RS485_DE = 4;     // PD4  – TQFP 25 – net RS485_DE (DE and /RE tied: HIGH = drive the bus)

// --- I2C bus: VL53L1CB ToF (0x29) behind the TXS0104E
//   SDA = PD1 (D2), SCL = PD0 (D3): hardware TWI via Wire

// --- Not used by this firmware
//   PF0 (A5) = Imotor: current sensor U7 is not populated → ignored by design
//   VL53L1CB GPIO1 only reaches test point TP14
//   PB1 / PB2 / PB3 (SCK / MOSI / MISO) only go to the ISP header J2

}  // namespace pins
