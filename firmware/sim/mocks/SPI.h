#pragma once
#include <stdint.h>
#define MSBFIRST 1
#define SPI_MODE0 0
#include <stddef.h>
struct SPISettings { SPISettings(uint32_t, int, int) {} };
void simSpiByte(uint8_t b);                  // implemented by the simulation: feeds the GC9A01 emulator
class SPIClass {
 public:
  void begin() {}
  void beginTransaction(SPISettings) {}
  bool setSCK(uint8_t) { return true; }
  bool setTX(uint8_t) { return true; }
  uint8_t transfer(uint8_t b) { simSpiByte(b); return 0; }
  void transfer(const void* tx, void* rx, size_t n) {
    const uint8_t* p = (const uint8_t*)tx;
    for (size_t i = 0; i < n; i++) { simSpiByte(p[i]); if (rx) ((uint8_t*)rx)[i] = 0; }
  }
};
extern SPIClass SPI;
