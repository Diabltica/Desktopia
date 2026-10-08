#pragma once
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define strlen_P(p) strlen((const char*)(p))
