#include "check.h"

// Mocks for dependencies used by scenes.cpp
#include "bulb_registry.h"
#include <cstdint>

// Define missing dependencies missing from test context
void bulbApplyState(Bulb * /*bulb*/, const BulbState & /*wanted*/, uint16_t /*transitionDs*/) {}

// Mocks provided by test_config_backup or we redefine them specifically
#define scenesCount mock_scenesCount_scenes
#define sceneNameAt mock_sceneNameAt_scenes
#define sceneEntryAt mock_sceneEntryAt_scenes
#define sceneImport mock_sceneImport_scenes
#define isValidSceneName mock_isValidSceneName_scenes

#define registryCount mock_registryCount_scenes
#define registryGet mock_registryGet_scenes
#define serializeBulbState mock_serializeBulbState_scenes
#define deserializeBulbState mock_deserializeBulbState_scenes
#define registryFlush mock_registryFlush_scenes

size_t mock_registryCount_scenes() { return 0; }
Bulb* mock_registryGet_scenes(size_t /*index*/) { return nullptr; }
void mock_registryFlush_scenes() {}

String mock_serializeBulbState_scenes(const BulbState & /*state*/) { return ""; }
bool mock_deserializeBulbState_scenes(BulbState & /*state*/, const String & /*data*/) { return true; }

// Directly include scenes.cpp to compile its contents
#include "../scenes.cpp"

void runSceneTests() {
  check::begin("scenes: isValidSceneName");

  // Valid names
  CHECK(mock_isValidSceneName_scenes("a"));
  CHECK(mock_isValidSceneName_scenes("z"));
  CHECK(mock_isValidSceneName_scenes("0"));
  CHECK(mock_isValidSceneName_scenes("9"));
  CHECK(mock_isValidSceneName_scenes("-"));
  CHECK(mock_isValidSceneName_scenes("_"));
  CHECK(mock_isValidSceneName_scenes("a0-_"));
  CHECK(mock_isValidSceneName_scenes("my-scene"));
  CHECK(mock_isValidSceneName_scenes("my_scene"));
  CHECK(mock_isValidSceneName_scenes("1234567890"));
  CHECK(mock_isValidSceneName_scenes("a1b2c3d4e5"));

  // Boundary conditions (length)
  CHECK(!mock_isValidSceneName_scenes("")); // Empty string
  CHECK(mock_isValidSceneName_scenes("12345678901234567890")); // Exact max length (20)
  CHECK(!mock_isValidSceneName_scenes("123456789012345678901")); // Exceeds max length (21)

  // Invalid characters
  CHECK(!mock_isValidSceneName_scenes(" ")); // Space
  CHECK(!mock_isValidSceneName_scenes("A")); // Uppercase
  CHECK(!mock_isValidSceneName_scenes("my scene"));
  CHECK(!mock_isValidSceneName_scenes("My-Scene"));
  CHECK(!mock_isValidSceneName_scenes("my-scene!"));
  CHECK(!mock_isValidSceneName_scenes("my@scene"));
  CHECK(!mock_isValidSceneName_scenes("scene/1"));
}
