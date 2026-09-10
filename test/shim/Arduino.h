#pragma once

// ---------------------------------------------------------------------------
// Host-test stand-in for <Arduino.h>.
//
// Provides only what the pure-logic headers need (String and the <cctype>
// helpers used by json_lite.h). Firmware includes the real core header; this
// one is reachable only because test/run.* puts test/shim first on the include
// path.
// ---------------------------------------------------------------------------

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "WString.h"
