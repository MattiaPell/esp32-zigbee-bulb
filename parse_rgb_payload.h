#pragma once

#include <Arduino.h>
#include "json_lite.h"

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

// Refactored from mqtt_bridge.cpp to allow unit testing of this internal logic
inline bool parseRgbPayload(const String &payload, uint8_t &r, uint8_t &g, uint8_t &bl) {
  const int at = payload.indexOf("\"color\"");
  if (at < 0) return false;
  const int brace = payload.indexOf('{', at);
  if (brace < 0) return false;
  const String obj = payload.substring(brace);
  long rr = 0, gg = 0, bb = 0;
  if (!jsonGetInt(obj, "r", rr) || !jsonGetInt(obj, "g", gg) ||
      !jsonGetInt(obj, "b", bb)) {
    return false;
  }
  r = (uint8_t)constrain(rr, 0, 255);
  g = (uint8_t)constrain(gg, 0, 255);
  bl = (uint8_t)constrain(bb, 0, 255);
  return true;
}
