#pragma once
#include <cstring>
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#define pgm_read_word(p) (*reinterpret_cast<const uint16_t*>(p))
#define pgm_read_dword(p) (*reinterpret_cast<const uint32_t*>(p))
