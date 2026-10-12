#include "check.h"
#include <cstdint>
#include <string>
#include <vector>

// Macros to rename conflicting symbols for mocking
#define millis millis_effects
#define debugLogPrintf debugLogPrintf_effects
#define registryCount registryCount_effects
#define registryGet registryGet_effects
#define bulbReady bulbReady_effects
#define bulbSendColorLoop bulbSendColorLoop_effects
#define randomSeed randomSeed_effects
#define random random_effects
#define bulbSendAllKelvin bulbSendAllKelvin_effects
#define bulbSendAllBrightness bulbSendAllBrightness_effects

// Forward declarations for mocks
void debugLogPrintf(const char* /*format*/, ...) {}
uint32_t mock_millis_effects = 0;
uint32_t millis() { return mock_millis_effects; }

// Include bulb registry to get the real structs
#include "../bulb_registry.h"

std::vector<Bulb> mock_effects_registry;
size_t registryCount() { return mock_effects_registry.size(); }
Bulb* registryGet(size_t i) { return i < mock_effects_registry.size() ? &mock_effects_registry[i] : nullptr; }

std::vector<const Bulb*> mock_effects_ready_bulbs;
bool bulbReady(const Bulb* bulb) {
  for (auto* b : mock_effects_ready_bulbs) {
    if (b == bulb) return true;
  }
  return false;
}

int mock_effects_color_loop_calls = 0;
bool mock_effects_last_color_loop_on = false;
void bulbSendColorLoop(Bulb* /*bulb*/, bool on) {
  mock_effects_color_loop_calls++;
  mock_effects_last_color_loop_on = on;
}

void randomSeed(unsigned long) {}
long random(long min, long /*max*/) { return min; }

int mock_effects_all_kelvin_calls = 0;
int bulbSendAllKelvin(int, uint16_t) { mock_effects_all_kelvin_calls++; return 0; }

int mock_effects_all_brightness_calls = 0;
int bulbSendAllBrightness(uint8_t, uint16_t) { mock_effects_all_brightness_calls++; return 0; }

// Include the source directly
#include "../effects.cpp"

namespace {

void resetMocks() {
  mock_millis_effects = 0;
  mock_effects_registry.clear();
  mock_effects_ready_bulbs.clear();
  mock_effects_color_loop_calls = 0;
  mock_effects_last_color_loop_on = false;
  mock_effects_all_kelvin_calls = 0;
  mock_effects_all_brightness_calls = 0;
  effectsBegin();
}

void testEffectStart() {
  check::begin("effectStart");

  // Empty string -> stop and return true
  resetMocks();
  mock_effects_registry.resize(1);
  mock_effects_ready_bulbs.push_back(&mock_effects_registry[0]);
  effectStart("color_loop");
  CHECK(effectActive() == "color_loop");

  CHECK(effectStart(""));
  CHECK(effectActive() == "none");

  // "none" -> stop and return true
  resetMocks();
  mock_effects_registry.resize(1);
  mock_effects_ready_bulbs.push_back(&mock_effects_registry[0]);
  effectStart("color_loop");
  CHECK(effectStart("none"));
  CHECK(effectActive() == "none");

  // "NONE" -> case-insensitive
  resetMocks();
  mock_effects_registry.resize(1);
  mock_effects_ready_bulbs.push_back(&mock_effects_registry[0]);
  effectStart("color_loop");
  CHECK(effectStart("NONE"));
  CHECK(effectActive() == "none");

  // Unknown effect
  resetMocks();
  CHECK(!effectStart("unknown"));
  CHECK(effectActive() == "none");

  // "color_loop" with no ready bulbs
  resetMocks();
  mock_effects_registry.resize(2); // 2 offline bulbs
  CHECK(!effectStart("color_loop"));
  CHECK(effectActive() == "none");
  CHECK_EQ(mock_effects_color_loop_calls, 0);

  // "color_loop" with ready bulbs
  resetMocks();
  mock_effects_registry.resize(2);
  mock_effects_ready_bulbs.push_back(&mock_effects_registry[0]);
  CHECK(effectStart("color_loop"));
  CHECK(effectActive() == "color_loop");
  CHECK_EQ(mock_effects_color_loop_calls, 1);
  CHECK(mock_effects_last_color_loop_on);

  // "candle" with no ready bulbs
  resetMocks();
  mock_effects_registry.resize(1);
  CHECK(!effectStart("candle"));
  CHECK(effectActive() == "none");

  // "candle" with ready bulbs
  resetMocks();
  mock_effects_registry.resize(1);
  mock_effects_ready_bulbs.push_back(&mock_effects_registry[0]);
  CHECK(effectStart("candle"));
  CHECK(effectActive() == "candle");
}

} // namespace

void runEffectsTests() {
  testEffectStart();
}
