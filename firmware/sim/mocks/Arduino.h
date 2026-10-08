#pragma once
// Minimal host-side stand-in for the Arduino AVR core (simulation only)
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "avr/pgmspace.h"
#include "avr/io.h"

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HEX 16
#define DEC 10
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
typedef bool boolean;
typedef uint8_t byte;

unsigned long millis();
void delay(unsigned long ms);
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t level);
int digitalRead(uint8_t pin);

struct SerialMock {
  void begin(unsigned long) {}
  size_t print(const char* s) { fputs(s, stdout); return 0; }
  size_t print(const __FlashStringHelper* s) { fputs((const char*)s, stdout); return 0; }
  size_t print(long v, int base = 10) { printf(base == 16 ? "%lx" : "%ld", v); return 0; }
  size_t print(unsigned long v, int base = 10) { printf(base == 16 ? "%lx" : "%lu", v); return 0; }
  size_t print(int v, int base = 10) { return print((long)v, base); }
  size_t print(unsigned v, int base = 10) { return print((unsigned long)v, base); }
  size_t print(unsigned char v, int base = 10) { return print((unsigned long)v, base); }
  template <typename T> size_t println(T v) { print(v); puts(""); return 0; }
  template <typename T> size_t println(T v, int base) { print(v, base); puts(""); return 0; }
};
extern SerialMock Serial;
