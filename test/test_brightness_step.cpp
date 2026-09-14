// Tests for brightness_step.h: relative brightness step with clamping.

#include "brightness_step.h"
#include "check.h"

void runBrightnessStepTests() {
  check::begin("brightness step");

  CHECK_EQ(brightnessStepClamp(50, 10), 60);
  CHECK_EQ(brightnessStepClamp(50, -10), 40);
  CHECK_EQ(brightnessStepClamp(50, 0), 50);
  CHECK_EQ(brightnessStepClamp(95, 10), 100);  // clamps at full brightness
  CHECK_EQ(brightnessStepClamp(5, -10), 1);    // never reaches 0 (that is "off")
  CHECK_EQ(brightnessStepClamp(0, 10), 10);    // stepping up from off works
  CHECK_EQ(brightnessStepClamp(100, 10), 100);
  CHECK_EQ(brightnessStepClamp(1, -10), 1);
}
