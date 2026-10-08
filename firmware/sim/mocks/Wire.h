#pragma once
#include <stdint.h>
#include <stddef.h>
class TwoWire {
 public:
  void begin() {}
  void setClock(uint32_t) {}
  void setWireTimeout(uint32_t = 25000, bool = false) {}
  void beginTransmission(uint8_t addr);
  size_t write(uint8_t b);
  uint8_t endTransmission(uint8_t sendStop = 1);
  uint8_t requestFrom(uint8_t addr, uint8_t qty);
  int read();
  int available();
 private:
  uint8_t _addr = 0, _tx[8], _txLen = 0, _rx[8], _rxLen = 0, _rxPos = 0;
  friend struct WireBackdoor;
};
extern TwoWire Wire;
