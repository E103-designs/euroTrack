#pragma once
#include <array>
#include <cstdint>
struct EEPROMStub {
  std::array<uint8_t, 1024> bytes;
  unsigned writes = 0;
  EEPROMStub() {
    bytes.fill(255);
  }
  uint8_t read(uint16_t address) {
    return bytes.at(address);
  }
  void update(uint16_t address, uint8_t value) {
    bytes.at(address) = value;
    ++writes;
  }
};
inline EEPROMStub EEPROM;
