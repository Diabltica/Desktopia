#pragma once
#include <stdint.h>
#define MSBFIRST 1
#define SPI_MODE0 0
struct SPISettings { SPISettings(uint32_t, int, int) {} };
class SPIClass { public: void begin() {} void beginTransaction(SPISettings) {} };
extern SPIClass SPI;
