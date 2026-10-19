#pragma once
/*
 * Desktopia HMI board – pin map extracted from desktopia_HMI.kicad_sch
 * RP2040 GPIO numbers (GPn).
 */
#include <stdint.h>

namespace hmipins {

// --- RS-485 transceiver U2 (SP3485EN) → RJ11 jack J2 → motherboard
constexpr uint8_t LINK_TX = 0;      // GP0 – UART0 TX → D
constexpr uint8_t LINK_RX = 1;      // GP1 – UART0 RX ← R
constexpr uint8_t LINK_DE = 2;      // GP2 – DE and /RE tied: HIGH = drive the bus

// --- Buttons SW1..SW5: to GND when pressed, internal pull-ups
constexpr uint8_t BTN_UP = 11;      // GP11 – SW1
constexpr uint8_t BTN_DOWN = 12;    // GP12 – SW2
constexpr uint8_t BTN_PRESET1 = 13; // GP13 – SW3
constexpr uint8_t BTN_PRESET2 = 14; // GP14 – SW4
constexpr uint8_t BTN_PRESET3 = 15; // GP15 – SW5

// --- Round LCD J1 (GC9A01), SPI0, write-only
constexpr uint8_t LCD_CS = 17;      // GP17 – SPI0 CSn (driven as a GPIO)
constexpr uint8_t LCD_SCK = 18;     // GP18 – SPI0 SCK
constexpr uint8_t LCD_MOSI = 19;    // GP19 – SPI0 TX
constexpr uint8_t LCD_DC = 20;      // GP20
constexpr uint8_t LCD_RST = 21;     // GP21

}  // namespace hmipins
