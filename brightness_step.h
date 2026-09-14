#pragma once

// ---------------------------------------------------------------------------
// brightness_step: pure helper for relative brightness changes.
//
// A "step" is a delta applied to the current brightness percentage. The
// result is clamped so a step never turns the bulb off (0 would be sent as
// off) and never exceeds full brightness. Header-only so the host tests can
// cover it without the ESP32 toolchain.
// ---------------------------------------------------------------------------

constexpr int BRIGHTNESS_STEP_MIN_PCT = 1;
constexpr int BRIGHTNESS_STEP_MAX_PCT = 100;

constexpr int brightnessStepClamp(int currentPct, int delta) {
  int value = currentPct + delta;
  if (value < BRIGHTNESS_STEP_MIN_PCT) value = BRIGHTNESS_STEP_MIN_PCT;
  if (value > BRIGHTNESS_STEP_MAX_PCT) value = BRIGHTNESS_STEP_MAX_PCT;
  return value;
}
