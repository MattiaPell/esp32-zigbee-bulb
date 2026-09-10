// Tests for adaptive_curve.h: the pure circadian interpolation.

#include "check.h"
#include "adaptive_curve.h"

namespace {

void expectCurve(int minute, int kelvin, int brightness) {
  int k = 0, b = 0;
  adaptiveAtMinute(minute, k, b);
  CHECK_EQ(k, kelvin);
  CHECK_EQ(b, brightness);
}

}  // namespace

void runAdaptiveTests() {
  check::begin("adaptive curve");

  expectCurve(0, 2200, 5);      // midnight
  expectCurve(390, 2400, 20);   // 06:30 key frame
  expectCurve(480, 3500, 70);   // 08:00 key frame
  expectCurve(720, 4000, 100);  // noon
  expectCurve(1020, 3500, 85);  // 17:00 key frame
  expectCurve(195, 2300, 12);   // midpoint of the night -> morning ramp
  expectCurve(1440, 2200, 5);   // wraps to midnight
  expectCurve(-60, 2267, 19);   // 23:00 on the previous day
}
