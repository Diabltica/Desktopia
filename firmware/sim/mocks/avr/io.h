#pragma once
#include <stdint.h>
#define _BV(b) (1 << (b))
// Timer1 / ports used by motor.cpp
extern volatile uint8_t TCCR1A, TCCR1B, PORTB;
extern volatile uint16_t ICR1, OCR1B;
#define COM1B1 5
#define WGM10 0
#define WGM11 1
#define WGM12 3
#define WGM13 4
#define CS10 0
#define PORTB6 6
// SPI used by gc9a01.cpp: every byte written to SPDR goes to the LCD emulator
struct SpdrProxy { SpdrProxy& operator=(uint8_t b); };
extern SpdrProxy SPDR;
extern volatile uint8_t SPSR;
#define SPIF 7
