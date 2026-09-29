#pragma once
#include "WString.h"
class Preferences {
public:
    Preferences() {}
    bool begin(const char *, bool=false) { return true; }
    void end() {}
    String getString(const char *, const char * defaultValue) { return String(defaultValue); }
    uint8_t getUChar(const char *, uint8_t defaultValue) { return defaultValue; }
    void putString(const char *, const String &) {}
    void putUChar(const char *, uint8_t) {}
    void remove(const char *) {}
};
