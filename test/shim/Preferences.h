#pragma once

#include "WString.h"

class Preferences {
public:
    Preferences() {}
    bool begin(const char * /*name*/, bool /*readOnly*/=false) { return true; }
    void end() {}

    uint8_t getUChar(const char*, uint8_t def) { return def; }
    void putUChar(const char*, uint8_t) {}
    void getBytes(const char*, void*, size_t) {}
    void putBytes(const char*, const void*, size_t) {}
    String getString(const char * /*key*/, const char * defaultValue) { return String(defaultValue); }
    void putString(const char*, const String&) {}
};
