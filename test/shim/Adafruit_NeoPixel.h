#pragma once

#include <cstdint>

#define NEO_GRB 0
#define NEO_KHZ800 0

extern uint32_t mock_pixel_color;
extern bool mock_pixel_show_called;
extern bool mock_pixel_begin_called;
extern bool mock_pixel_clear_called;
extern uint8_t mock_pixel_brightness;

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(uint16_t n, uint8_t p, uint8_t t)
      : numLEDs(n), pin(p), type(t) {}

  void begin() {
    mock_pixel_begin_called = true;
  }

  void setBrightness(uint8_t b) {
    mock_pixel_brightness = b;
  }

  void clear() {
    mock_pixel_clear_called = true;
  }

  void setPixelColor(uint16_t n, uint32_t c) {
    if (n == 0) mock_pixel_color = c;
  }

  uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }

  void show() {
    mock_pixel_show_called = true;
  }

private:
  uint16_t numLEDs;
  uint8_t pin;
  uint8_t type;
};
