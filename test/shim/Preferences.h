#pragma once

#include "WString.h"

class Preferences {
public:
    Preferences() {}
    bool begin(const char * /*name*/, bool /*readOnly*/=false) { return true; }
    void end() {}
    String getString(const char * /*key*/, const char * defaultValue) { return String(defaultValue); }
};
