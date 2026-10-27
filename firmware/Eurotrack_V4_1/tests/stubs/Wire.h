#pragma once
#include "Arduino.h"
struct WireStub {
  uint8_t status = 0;
  bool timeout = false;
  std::vector<uint8_t> tx;
  std::vector<std::vector<uint8_t>> transactions;
  void begin() {}
  void setClock(uint32_t) {}
  void setWireTimeout(uint32_t, bool) {}
  void beginTransmission(uint8_t) {
    tx.clear();
  }
  size_t write(uint8_t value) {
    tx.push_back(value);
    return 1;
  }
  size_t write(const uint8_t *p, size_t n) {
    tx.insert(tx.end(), p, p + n);
    return n;
  }
  uint8_t endTransmission() {
    transactions.push_back(tx);
    return status;
  }
  bool getWireTimeoutFlag() {
    return timeout;
  }
  void clearWireTimeoutFlag() {
    timeout = false;
  }
};
inline WireStub Wire;
