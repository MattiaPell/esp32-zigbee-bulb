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

#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

#ifndef HEX
#define HEX 16
#endif

// The firmware uses strlcpy from newlib; glibc and mingw-w64 don't ship it, so
// the BSD/macOS family (which does) is the only target that must not redefine it.
#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
inline size_t strlcpy(char *dst, const char *src, size_t size) {
  size_t copied = 0;
  if (size > 0) {
    for (; copied + 1 < size && src[copied] != '\0'; ++copied) dst[copied] = src[copied];
    dst[copied] = '\0';
  }
  size_t len = 0;
  while (src[len] != '\0') ++len;
  return len;
}
#endif
