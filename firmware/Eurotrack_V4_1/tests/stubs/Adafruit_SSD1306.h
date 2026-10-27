#pragma once
#include <Adafruit_GFX.h>
#include "Wire.h"
#define WHITE 1
#define BLACK 0
#define SSD1306_SWITCHCAPVCC 2
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COLUMNADDR 0x21
class Adafruit_SSD1306 : public Adafruit_GFX {
public:
  uint8_t pixels[1024] = {};
  Adafruit_SSD1306(uint8_t w, uint8_t h, WireStub *, int8_t, uint32_t, uint32_t)
      : Adafruit_GFX(w, h) {}
  bool begin(uint8_t, uint8_t, bool, bool) {
    return true;
  }
  void clearDisplay() {
    memset(pixels, 0, sizeof(pixels));
  }
  uint8_t *getBuffer() {
    return pixels;
  }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= 128 || y < 0 || y >= 64)
      return;
    uint8_t &b = pixels[x + (y / 8) * 128];
    uint8_t mask = 1 << (y % 8);
    if (color)
      b |= mask;
    else
      b &= ~mask;
  }
};
