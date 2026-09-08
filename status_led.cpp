#include "status_led.h"

#include <Adafruit_NeoPixel.h>

#include "config.h"

namespace {

Adafruit_NeoPixel pixel(1, STATUS_PIXEL_PIN, NEO_GRB + NEO_KHZ800);
StatusLedMode mode = StatusLedMode::Off;

void setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

}  // namespace

void statusLedBegin() {
  pixel.begin();
  pixel.setBrightness(STATUS_LED_BRIGHTNESS);
  pixel.clear();
  pixel.show();
}

void statusLedSetMode(StatusLedMode newMode) {
  if (newMode == mode) return;
  mode = newMode;
}

StatusLedMode statusLedGetMode() {
  return mode;
}

void statusLedTick() {
  const uint32_t now = millis();
  switch (mode) {
    case StatusLedMode::Off:
      setColor(0, 0, 0);
      break;

    case StatusLedMode::WifiConnecting:  // Blinking blue.
      if ((now / 400) % 2 == 0) setColor(0, 0, 0);
      else setColor(0, 0, 60);
      break;

    case StatusLedMode::ZigbeeStarting:  // Blinking purple.
      if ((now / 400) % 2 == 0) setColor(0, 0, 0);
      else setColor(45, 0, 45);
      break;

    case StatusLedMode::Pairing: {  // Breathing amber.
      const float phase = (now % 2200) / 2200.0f * 2.0f * PI;
      const uint8_t v = (uint8_t)(40.0f * (0.5f + 0.5f * sinf(phase)));
      setColor(v, (uint8_t)(v * 0.45f), 0);
      break;
    }

    case StatusLedMode::Ready:  // Solid green.
      setColor(0, 40, 0);
      break;

    case StatusLedMode::Error:  // Fast blinking red.
      setColor((now / 120) % 2 == 0 ? 45 : 0, 0, 0);
      break;
  }
}
