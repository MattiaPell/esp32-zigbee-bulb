#pragma once

// ---------------------------------------------------------------------------
// Tiny assertion framework for host-side unit tests. No external deps.
//
// Usage:
//   check::begin("case name");
//   CHECK(condition);
//   CHECK_EQ(a, b);
// The process exit code is set by the runner (see run_tests.cpp).
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>
#include <type_traits>

#include "WString.h"

namespace check {

inline int &checks() {
  static int value = 0;
  return value;
}

inline int &failures() {
  static int value = 0;
  return value;
}

inline const char *&currentCase() {
  static const char *value = "";
  return value;
}

inline void begin(const char *name) { currentCase() = name; }

inline void reportFail(const char *what, const char *detail, const char *file, int line) {
  ++checks();
  ++failures();
  std::printf("  FAIL [%s] %s: %s (%s:%d)\n", currentCase(), what, detail, file, line);
}

template <typename T>
std::string toStr(const T &value) {
  if constexpr (std::is_same_v<T, String>) {
    return std::string(value.c_str());
  } else if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_same_v<T, std::string>) {
    return value;
  } else if constexpr (std::is_convertible_v<T, long long>) {
    return std::to_string(static_cast<long long>(value));
  } else {
    return std::string(value);
  }
}

template <typename A, typename B>
void eq(const A &a, const B &b, const char *ea, const char *eb, const char *file, int line) {
  if (a == b) {
    ++checks();
    return;
  }
  const std::string detail = toStr(a) + " != " + toStr(b);
  const std::string what = std::string(ea) + " == " + eb;
  reportFail(what.c_str(), detail.c_str(), file, line);
}

inline void truth(bool ok, const char *expr, const char *file, int line) {
  if (ok) {
    ++checks();
    return;
  }
  reportFail(expr, "expected true", file, line);
}

}  // namespace check

#define CHECK(cond) check::truth((cond), #cond, __FILE__, __LINE__)
#define CHECK_EQ(a, b) check::eq((a), (b), #a, #b, __FILE__, __LINE__)
