#pragma once
#include "Arduino.h"
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  size_t write(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; ++i)
      write(data[i]);
    return size;
  }
  size_t write(const char *s) {
    return write(reinterpret_cast<const uint8_t *>(s), strlen(s));
  }
  size_t print(char *s) {
    return write(s);
  }
  size_t print(const char *s) {
    return write(s);
  }
  size_t print(const __FlashStringHelper *s) {
    return write(reinterpret_cast<const char *>(s));
  }
  size_t print(char c) {
    return write(c);
  }
  template <class T> size_t print(T value) {
    return write(std::to_string(value).c_str());
  }
};
