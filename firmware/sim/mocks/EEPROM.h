#pragma once
#include <stdint.h>
#include <string.h>
struct EEPROMClass {
  uint8_t mem[1024];
  unsigned writes = 0;
  EEPROMClass() { memset(mem, 0xFF, sizeof(mem)); }
  uint8_t read(int i) { return mem[i]; }
  void write(int i, uint8_t v) { mem[i] = v; writes++; }
  void update(int i, uint8_t v) { if (mem[i] != v) write(i, v); }
  template <typename T> T& get(int idx, T& t) { memcpy(&t, mem + idx, sizeof(T)); return t; }
  template <typename T> const T& put(int idx, const T& t) {
    const uint8_t* p = (const uint8_t*)&t;
    for (unsigned i = 0; i < sizeof(T); i++) update(idx + (int)i, p[i]);
    return t;
  }
};
extern EEPROMClass EEPROM;
