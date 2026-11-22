#pragma once
#include <stdint.h>
#include <string.h>
// arduino-pico EEPROM emulation: a RAM copy (mem) written to its flash sector (_EEPROM_start, defined in sim.cpp) by
// commit(). The simulation keeps mem as the persistent image: begin() does not reload it.
extern "C" uint8_t _EEPROM_start[];
struct EEPROMClass {
  uint8_t mem[1024];
  unsigned writes = 0;                      // bytes changed by put() / write()
  unsigned commits = 0;                     // flash sector erase + program
  EEPROMClass() { memset(mem, 0xFF, sizeof(mem)); }
  void begin(size_t) {}
  bool commit() { memcpy(_EEPROM_start, mem, sizeof(mem)); commits++; return true; }
  const uint8_t* getConstDataPtr() const { return mem; }
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
