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

// --- HMI board link: RS-485 transceiver U8 (SP485EEN), RJ11 jack J6
//   RX = PD2 (D0) – TQFP 20 – net RS485_RO      TX = PD3 (D1) – TQFP 21 – net RS485_DI   → Serial1
constexpr uint8_t RS485_DE = 4;     // PD4  – TQFP 25 – net RS485_DE (DE and /RE tied: HIGH = drive the bus)

// --- I2C bus: VL53L1CB ToF (0x29) behind the TXS0104E, INA226 motor current monitor U9 (0x40) on the 5 V side
//   SDA = PD1 (D2), SCL = PD0 (D3): hardware TWI via Wire

// --- Not used by this firmware
//   PE6 (D7) = I_ALERT: INA226 alert output, open drain, 10k pull-up R26 (INT6)
//   PF0 (A5) is now free (it read the old analog current sensor)
//   VL53L1CB GPIO1 only reaches test point TP14
//   PB1 / PB2 / PB3 (SCK / MOSI / MISO) only go to the ISP header J2

}  // namespace pins
