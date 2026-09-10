// Invariants for the built-in preset table (presets.h): the ids are used as
// API path segments, so they must be URL-safe, unique, and the values sane.

#include "check.h"
#include "presets.h"

namespace {

bool urlSafe(const char *s) {
  if (s == nullptr || s[0] == '\0') return false;
  for (const char *p = s; *p != '\0'; ++p) {
    const char c = *p;
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_')) {
      return false;
    }
  }
  return true;
}

}  // namespace

void runPresetTests() {
  check::begin("presets table");

  CHECK(LIGHT_PRESET_COUNT > 0);
  for (size_t i = 0; i < LIGHT_PRESET_COUNT; ++i) {
    CHECK(urlSafe(LIGHT_PRESETS[i].id));
    CHECK(LIGHT_PRESETS[i].name != nullptr && LIGHT_PRESETS[i].name[0] != '\0');
    CHECK(LIGHT_PRESETS[i].brightness >= 1 && LIGHT_PRESETS[i].brightness <= 100);
    CHECK(LIGHT_PRESETS[i].kelvin > 0);
    for (size_t j = i + 1; j < LIGHT_PRESET_COUNT; ++j) {
      CHECK(!String(LIGHT_PRESETS[i].id).equalsIgnoreCase(LIGHT_PRESETS[j].id));
    }
  }
}
