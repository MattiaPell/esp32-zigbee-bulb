#pragma once
#include "WString.h"
#include <cstdint>

// Host-side stub of the Arduino Preferences API: everything is a no-op that
// echoes back what it was given, so tests can exercise call sequences without
// touching NVS. Signatures mirror the real library closely enough for the
// firmware call sites compiled into the tests.
class Preferences {
public:
  Preferences() {}
  bool begin(const char *, bool = false) { return true; }
  void end() {}

  bool remove(const char *) { return true; }

  bool getBool(const char *, bool defaultValue = false) { return defaultValue; }
  size_t putBool(const char *, bool) { return 0; }

  int32_t getInt(const char *, int32_t defaultValue = 0) { return defaultValue; }
  size_t putInt(const char *, int32_t) { return 0; }

  uint8_t getUChar(const char *, uint8_t defaultValue = 0) { return defaultValue; }
  size_t putUChar(const char *, uint8_t) { return 0; }

  String getString(const char *, const char *defaultValue = "") {
    return String(defaultValue);
  }
  String getString(const char *, const String &defaultValue) { return defaultValue; }
  size_t putString(const char *, const String &) { return 0; }
  size_t putString(const char *, const char *) { return 0; }

  size_t getBytes(const char *, void *, size_t) { return 0; }
  size_t putBytes(const char *, const void *, size_t) { return 0; }
};
