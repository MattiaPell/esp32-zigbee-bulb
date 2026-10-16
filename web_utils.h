#pragma once

#include <Arduino.h>

bool isValidHostname(const String &name);

bool parseRgbHex(const String &rgbHex, uint8_t &r, uint8_t &g, uint8_t &b);
