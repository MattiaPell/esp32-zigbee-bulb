#include "check.h"
#include <cstdint>

// Forward declare constrain
#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

// Redirect mocks to avoid multiple definitions when compiled with test_light_timers.cpp
#define registryFindByIeee dummy_registryFindByIeee
#define bulbIeeeHex dummy_bulbIeeeHex

// Need to forward declare millis for bulb_registry.cpp to compile cleanly
uint32_t millis();

#include "../bulb_registry.cpp"

void test_deserializeBulbState() {
  check::begin("deserializeBulbState");
  BulbState st;

  // Happy path - White mode (mode 0)
  st = BulbState();
  CHECK(deserializeBulbState(st, "0,1,128,3000,255,255,255"));
  CHECK_EQ((uint8_t)st.mode, (uint8_t)BulbColorMode::White);
  CHECK_EQ(st.power, true);
  CHECK_EQ(st.level, 128);
  CHECK_EQ(st.kelvin, 3000);
  CHECK_EQ(st.red, 255);
  CHECK_EQ(st.green, 255);
  CHECK_EQ(st.blue, 255);

  // Happy path - RGB mode (mode 1)
  st = BulbState();
  CHECK(deserializeBulbState(st, "1,0,255,2200,10,20,30"));
  CHECK_EQ((uint8_t)st.mode, (uint8_t)BulbColorMode::Rgb);
  CHECK_EQ(st.power, false);
  CHECK_EQ(st.level, 255);
  CHECK_EQ(st.kelvin, 2200);
  CHECK_EQ(st.red, 10);
  CHECK_EQ(st.green, 20);
  CHECK_EQ(st.blue, 30);

  // Missing commas (less than 7 elements)
  st = BulbState();
  CHECK(!deserializeBulbState(st, "0,1,128,3000,255,255"));
  CHECK(!deserializeBulbState(st, "0,1"));
  CHECK(!deserializeBulbState(st, ""));

  // Empty values between commas (strtol parses empty as 0)
  st = BulbState();
  CHECK(deserializeBulbState(st, "0,,128,3000,255,255,255"));
  CHECK_EQ(st.power, false);

  // Constraints check (above bounds)
  st = BulbState();
  CHECK(deserializeBulbState(st, "0,1,300,5000,300,300,300"));
  CHECK_EQ(st.level, 255);
  CHECK_EQ(st.kelvin, MAX_KELVIN);
  CHECK_EQ(st.red, 255);
  CHECK_EQ(st.green, 255);
  CHECK_EQ(st.blue, 255);

  // Constraints check (below bounds)
  st = BulbState();
  CHECK(deserializeBulbState(st, "0,1,-10,1000,-10,-10,-10"));
  CHECK_EQ(st.level, 0);
  CHECK_EQ(st.kelvin, MIN_KELVIN);
  CHECK_EQ(st.red, 0);
  CHECK_EQ(st.green, 0);
  CHECK_EQ(st.blue, 0);
}

void runBulbRegistryTests() {
  test_deserializeBulbState();
}
