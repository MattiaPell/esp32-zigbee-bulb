#pragma once

#include <Arduino.h>

bool isValidHostname(const String &name);

bool parseRgbHex(const String &rgbHex, uint8_t &r, uint8_t &g, uint8_t &b);

struct Bulb;

struct PatchContext {
  int httpCode = 200;
  String errorMsg;
  bool changed = false;
};

// Applies a patch (JSON body) to a Bulb.
// Populates PatchContext with error details and http code.
// Returns true on success, false if an error occurred.
bool applyLightPatch(Bulb *b, const String &body, PatchContext &ctx);
