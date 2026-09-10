#pragma once

// ---------------------------------------------------------------------------
// adaptive_curve: the pure circadian mapping used by the adaptive lighting
// module. Kept free of Arduino headers so it can be unit-tested on the host.
//
// The curve is a list of key frames (local time of day -> white temperature
// and brightness) interpolated linearly. Outside the table the value wraps
// through midnight.
// ---------------------------------------------------------------------------

#include <stdint.h>

struct AdaptivePoint {
  uint16_t minuteOfDay;  // 0..1440
  uint16_t kelvin;
  uint8_t brightness;  // 0-100 %
};

inline constexpr AdaptivePoint ADAPTIVE_CURVE[] = {
    {0, 2200, 5},      // 00:00
    {390, 2400, 20},   // 06:30
    {480, 3500, 70},   // 08:00
    {720, 4000, 100},  // 12:00
    {1020, 3500, 85},  // 17:00
    {1200, 2800, 55},  // 20:00
    {1350, 2300, 25},  // 22:30
    {1440, 2200, 5},   // 24:00 (== 00:00)
};
inline constexpr int ADAPTIVE_CURVE_COUNT =
    sizeof(ADAPTIVE_CURVE) / sizeof(ADAPTIVE_CURVE[0]);

// Linear interpolation; minute is wrapped into [0, 1440).
inline void adaptiveAtMinute(int minute, int &kelvin, int &brightness) {
  minute %= 1440;
  if (minute < 0) minute += 1440;
  for (int i = 0; i + 1 < ADAPTIVE_CURVE_COUNT; ++i) {
    const int a = ADAPTIVE_CURVE[i].minuteOfDay;
    const int b = ADAPTIVE_CURVE[i + 1].minuteOfDay;
    if (minute < a || minute >= b) continue;
    const int span = b - a;
    const int frac = minute - a;
    kelvin = ADAPTIVE_CURVE[i].kelvin +
             (ADAPTIVE_CURVE[i + 1].kelvin - ADAPTIVE_CURVE[i].kelvin) * frac /
                 span;
    brightness = ADAPTIVE_CURVE[i].brightness +
                 (ADAPTIVE_CURVE[i + 1].brightness - ADAPTIVE_CURVE[i].brightness) *
                     frac / span;
    return;
  }
  kelvin = ADAPTIVE_CURVE[0].kelvin;
  brightness = ADAPTIVE_CURVE[0].brightness;
}
