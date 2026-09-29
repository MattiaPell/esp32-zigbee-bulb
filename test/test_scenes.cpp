#include "check.h"

// Mocks for dependencies used by scenes.cpp
#include "bulb_registry.h"
#include <cstdint>

// Mock variables
static size_t mockRegistryCount = 0;

size_t registryCount() { return mockRegistryCount; }
Bulb* registryGet(size_t /*index*/) { return nullptr; }
void registryFlush() {}

String serializeBulbState(const BulbState & /*state*/) { return ""; }
bool deserializeBulbState(BulbState & /*state*/, const String & /*data*/) { return true; }
void bulbApplyState(Bulb * /*bulb*/, const BulbState & /*wanted*/, uint16_t /*transitionDs*/) {}

// Directly include scenes.cpp to compile its contents
#include "../scenes.cpp"

void runSceneTests() {
  check::begin("scenes: isValidSceneName");

  // Valid names
  CHECK(isValidSceneName("a"));
  CHECK(isValidSceneName("z"));
  CHECK(isValidSceneName("0"));
  CHECK(isValidSceneName("9"));
  CHECK(isValidSceneName("-"));
  CHECK(isValidSceneName("_"));
  CHECK(isValidSceneName("a0-_"));
  CHECK(isValidSceneName("my-scene"));
  CHECK(isValidSceneName("my_scene"));
  CHECK(isValidSceneName("1234567890"));
  CHECK(isValidSceneName("a1b2c3d4e5"));

  // Boundary conditions (length)
  CHECK(!isValidSceneName("")); // Empty string
  CHECK(isValidSceneName("12345678901234567890")); // Exact max length (20)
  CHECK(!isValidSceneName("123456789012345678901")); // Exceeds max length (21)

  // Invalid characters
  CHECK(!isValidSceneName(" ")); // Space
  CHECK(!isValidSceneName("A")); // Uppercase
  CHECK(!isValidSceneName("my scene"));
  CHECK(!isValidSceneName("My-Scene"));
  CHECK(!isValidSceneName("my-scene!"));
  CHECK(!isValidSceneName("my@scene"));
  CHECK(!isValidSceneName("scene/1"));
}
