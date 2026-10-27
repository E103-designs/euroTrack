#pragma once
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define CHANGE 1
#define RISING 2
#define PROGMEM
class __FlashStringHelper;
#define F(s) reinterpret_cast<const __FlashStringHelper *>(s)
using String = std::string;
using boolean = bool;
template <class T> T min(T a, T b) {
  return a < b ? a : b;
}
template <class T> T max(T a, T b) {
  return a > b ? a : b;
}
inline uint64_t fakeUs = 0;
inline uint8_t pins[32] = {};
struct PinWrite {
  uint64_t us;
  uint8_t pin, value;
};
inline std::vector<PinWrite> pinWrites;
inline uint32_t millis() {
  return uint32_t(fakeUs / 1000);
}
inline uint32_t micros() {
  return uint32_t(fakeUs);
}
inline void digitalWrite(uint8_t pin, uint8_t value) {
  pins[pin] = value;
  pinWrites.push_back({fakeUs, pin, value});
}
inline int digitalRead(uint8_t pin) {
  return pins[pin];
}
inline void pinMode(uint8_t pin, uint8_t mode) {
  if (mode == INPUT_PULLUP)
    pins[pin] = HIGH;
}
inline int digitalPinToInterrupt(uint8_t pin) {
  return pin;
}
inline void attachInterrupt(int, void (*)(), int) {}
inline void yield() {}

inline float radians(float angle) {
  return angle * 0.017453292519943295f;
}
