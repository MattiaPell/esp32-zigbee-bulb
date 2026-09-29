#pragma once

#include "WString.h"
#include <cstdint>

class Preferences {
public:
    Preferences() {}
    bool begin(const char * /*name*/, bool /*readOnly*/=false) { return true; }
    void end() {}
    String getString(const char * /*key*/, const char * defaultValue) { return String(defaultValue); }
    uint8_t getUChar(const char * /*key*/, uint8_t defaultValue) { return defaultValue; }
    void putString(const char * /*key*/, const String & /*value*/) {}
    void putUChar(const char * /*key*/, uint8_t /*value*/) {}
    void remove(const char * /*key*/) {}
};
