#pragma once
#include "WString.h"
#include <cstdint>

class Preferences {
public:
    Preferences() {}
    bool begin(const char *, bool=false) { return true; }
    void end() {}

    uint8_t getUChar(const char*, uint8_t def) { return def; }
    void putUChar(const char*, uint8_t) {}
    void getBytes(const char*, void*, size_t) {}
    void putBytes(const char*, const void*, size_t) {}
    String getString(const char * /*key*/, const char * defaultValue) { return String(defaultValue); }
    uint8_t getUChar(const char * /*key*/, uint8_t defaultValue) { return defaultValue; }
    void putString(const char * /*key*/, const String & /*value*/) {}
    void putUChar(const char * /*key*/, uint8_t /*value*/) {}
    void remove(const char * /*key*/) {}
    uint8_t getUChar(const char *, uint8_t defaultValue) { return defaultValue; }
    size_t getBytes(const char *, void *, size_t) { return 0; }
    size_t putUChar(const char *, uint8_t) { return 0; }
    size_t putBytes(const char *, const void *, size_t) { return 0; }
    size_t putString(const char *, const String &) { return 0; }
    size_t putString(const char *, const char *) { return 0; }
    size_t putString(const char * /*key*/, const String & /*value*/) { return 0; }
    size_t putUChar(const char * /*key*/, uint8_t /*value*/) { return 0; }
    uint8_t getUChar(const char * /*key*/, uint8_t defaultValue) { return defaultValue; }
    bool remove(const char * /*key*/) { return true; }
};
