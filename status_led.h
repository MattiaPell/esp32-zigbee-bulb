#pragma once

// Onboard NeoPixel as a status light.
// Call statusLedBegin() once and statusLedTick() from the main loop; the
// patterns are driven by millis(), so nothing blocks.

#include <Arduino.h>

enum class StatusLedMode {
  Off,
  WifiConnecting,   // blinking blue
  ZigbeeStarting,   // blinking purple
  Pairing,          // breathing amber
  Ready,            // solid green
  Error,            // fast blinking red
};

void statusLedBegin();
void statusLedSetMode(StatusLedMode mode);
StatusLedMode statusLedGetMode();
void statusLedTick();
