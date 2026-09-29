#pragma once

#include "WString.h"

class Preferences {
public:
    Preferences() {}
    bool begin(const char * /*name*/, bool /*readOnly*/=false) { return true; }
    void end() {}
    String getString(const char * /*key*/, const char * defaultValue) { return String(defaultValue); }
    uint8_t getUChar(const char *, uint8_t defaultValue) { return defaultValue; }
    size_t getBytes(const char *, void *, size_t) { return 0; }
    size_t putUChar(const char *, uint8_t) { return 0; }
    size_t putBytes(const char *, const void *, size_t) { return 0; }
    size_t putString(const char *, const String &) { return 0; }
    size_t putString(const char *, const char *) { return 0; }
};
