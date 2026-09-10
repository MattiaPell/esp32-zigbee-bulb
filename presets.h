#pragma once

// ---------------------------------------------------------------------------
// presets: quick, built-in lighting looks ("Relax", "Focus", "Notte") applied
// to every bulb as a single tap — a uniform brightness + white temperature.
//
// Unlike scenes (which snapshot and later restore arbitrary per-bulb state),
// presets are fixed looks: not persisted, not user-editable. Their ids are
// used as the API path segment, so they stay URL-safe.
// ---------------------------------------------------------------------------

#include <Arduino.h>

struct LightPreset {
  const char *id;       // URL-safe identifier
  const char *name;     // Human-readable label
  uint8_t brightness;   // 0-100 %
  uint16_t kelvin;      // white temperature
};

inline constexpr LightPreset LIGHT_PRESETS[] = {
    {"relax", "Relax", 40, 2700},
    {"focus", "Focus", 100, 4000},
    {"notte", "Notte", 10, 2200},
};
inline constexpr size_t LIGHT_PRESET_COUNT =
    sizeof(LIGHT_PRESETS) / sizeof(LIGHT_PRESETS[0]);

// JSON array of {"id","name","brightness","kelvin"}.
String presetsJson();

// Applies the preset to every reachable bulb. Returns how many bulbs were
// commanded, or -1 if the id is unknown.
int presetApply(const String &id);
