#include "check.h"
#include <map>
#include <string>

// Include the header BEFORE the macro redefine so its actual class parses!
#include <Preferences.h>

// Mocks for dependencies used by scenes.cpp
#include "bulb_registry.h"
#include <cstdint>

class ScenesPreferencesMock {
public:
  std::map<std::string, std::string> strings;
  std::map<std::string, uint8_t> uchars;

  ScenesPreferencesMock() {}
  bool begin(const char *, bool = false) { return true; }
  void end() {}

  bool remove(const char *key) {
    strings.erase(key);
    uchars.erase(key);
    return true;
  }

  uint8_t getUChar(const char *key, uint8_t defaultValue = 0) {
    if (uchars.find(key) != uchars.end()) return uchars[key];
    return defaultValue;
  }
  size_t putUChar(const char *key, uint8_t val) {
    uchars[key] = val;
    return 1;
  }

  String getString(const char *key, const char *defaultValue = "") {
    if (strings.find(key) != strings.end()) return String(strings[key].c_str());
    return String(defaultValue);
  }
  size_t putString(const char *key, const String &val) {
    strings[key] = val.c_str();
    return val.length();
  }
};

#define Preferences ScenesPreferencesMock

// web_hooks mocking
#define webHookEvent mock_webHookEvent_scenes
bool mock_webHookEvent_scenes(const char* /*event*/, const char* /*id*/, const char* /*name*/, const char* /*scene*/) { return true; }

#define debugLogPrintf debugLogPrintf_scenes
#define debugLogPrintln debugLogPrintln_scenes
void debugLogPrintf_scenes(const char * /*format*/, ...) {}
void debugLogPrintln_scenes(const char * /*message*/) {}

// State dependencies
#define serializeBulbState mock_serializeBulbState_scenes
#define deserializeBulbState mock_deserializeBulbState_scenes
#define registryFlush mock_registryFlush_scenes

void mock_registryFlush_scenes() {}
String mock_serializeBulbState_scenes(const BulbState & state) {
  return String(state.power ? "on" : "off");
}
bool mock_deserializeBulbState_scenes(BulbState & state, const String & data) {
  if (data == "on") state.power = true;
  else if (data == "off") state.power = false;
  else return false;
  return true;
}

#define registryCount mock_registryCount_scenes
#define registryGet mock_registryGet_scenes
#define bulbIeeeHex mock_bulbIeeeHex_scenes
#define bulbReady mock_bulbReady_scenes

size_t mock_registry_count = 0;
Bulb mock_bulbs_scenes[5];

size_t mock_registryCount_scenes() { return mock_registry_count; }
Bulb* mock_registryGet_scenes(size_t index) { return &mock_bulbs_scenes[index]; }

String mock_bulbIeeeHex_scenes(const Bulb *b) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%08lx", (unsigned long)b);
  return String(buf);
}
bool mock_bulbReady_scenes(const Bulb *b) { return b->online; }

// Provide bulbApplyState globally
int mock_apply_calls_scenes = 0;
void bulbApplyState(Bulb * /*bulb*/, const BulbState & /*wanted*/, uint16_t /*transitionDs*/) {
  mock_apply_calls_scenes++;
}

// Since tests run together, we don't want scenes functions to conflict.
// If test_config_backup.cpp defines mocks for scene functions, we must rename the real ones in this test unit.
#define scenesBegin scenesBegin_real
#define scenesCount scenesCount_real
#define sceneNameAt sceneNameAt_real
#define sceneEntryAt sceneEntryAt_real
#define sceneImport sceneImport_real
#define isValidSceneName isValidSceneName_real
#define sceneCapture sceneCapture_real
#define sceneRecall sceneRecall_real
#define sceneDelete sceneDelete_real
#define sceneIndexOf sceneIndexOf_real

#include "../scenes.cpp"

void testSceneImport() {
  check::begin("scenes: sceneImport");

  mock_registry_count = 0;
  scenesBegin_real();

  // Test import invalid name
  CHECK(!sceneImport_real("invalid name", "some_data"));

  // Test valid name
  CHECK(sceneImport_real("scene-import", "some_data"));
  CHECK_EQ(scenesCount_real(), (size_t)1);
  CHECK_EQ(sceneNameAt_real(0), String("scene-import"));
  CHECK_EQ(sceneEntryAt_real(0), String("some_data"));

  // Clean up
  sceneDelete_real("scene-import");
  CHECK_EQ(scenesCount_real(), (size_t)0);
}

void testSceneEdgeCases() {
  check::begin("scenes: edge cases");

  mock_registry_count = 2;
  mock_bulbs_scenes[0].online = true;
  mock_bulbs_scenes[1].online = true;

  scenesBegin_real();

  // Test MAX_SCENES limit
  for (int i = 0; i < 16; ++i) { // MAX_SCENES is 16
    String name = "scene" + String(i);
    CHECK(sceneCapture_real(name));
  }
  CHECK_EQ(scenesCount_real(), (size_t)16);

  // Exceed MAX_SCENES
  CHECK(!sceneCapture_real("scene16"));

  // Delete all to clean up
  for (int i = 0; i < 16; ++i) {
    String name = "scene" + String(i);
    sceneDelete_real(name);
  }
  CHECK_EQ(scenesCount_real(), (size_t)0);

  // Recall non-existent scene
  int applied = 0, skipped = 0;
  CHECK(!sceneRecall_real("non-existent", applied, skipped));

  // Test skipping offline bulbs during recall
  CHECK(sceneCapture_real("scene_offline"));
  mock_bulbs_scenes[0].online = false; // Bulb 0 goes offline

  mock_apply_calls_scenes = 0;
  CHECK(sceneRecall_real("scene_offline", applied, skipped));
  CHECK_EQ(applied, 1);
  CHECK_EQ(skipped, 1);
  CHECK_EQ(mock_apply_calls_scenes, 1);

  sceneDelete_real("scene_offline");
}

void runSceneTests() {
  testSceneImport();
  testSceneEdgeCases();

  check::begin("scenes: capture and recall");

  // Setup
  mock_registry_count = 2;
  mock_bulbs_scenes[0].online = true;
  mock_bulbs_scenes[0].state.power = true;
  mock_bulbs_scenes[1].online = true;
  mock_bulbs_scenes[1].state.power = false;

  scenesBegin_real();

  CHECK(sceneCapture_real("scene1"));
  CHECK_EQ(scenesCount_real(), (size_t)1);
  CHECK_EQ(sceneNameAt_real(0), String("scene1"));

  // Change current states
  mock_bulbs_scenes[0].state.power = false;
  mock_bulbs_scenes[1].state.power = true;

  int applied = 0, skipped = 0;
  mock_apply_calls_scenes = 0;
  CHECK(sceneRecall_real("scene1", applied, skipped));
  CHECK_EQ(applied, 2);
  CHECK_EQ(skipped, 0);
  CHECK_EQ(mock_apply_calls_scenes, 2);

  // Check if they got restored
  CHECK(mock_bulbs_scenes[0].state.power);
  CHECK(!mock_bulbs_scenes[1].state.power);

  // Test scene delete
  sceneDelete_real("scene1");
  CHECK_EQ(scenesCount_real(), (size_t)0);
  CHECK_EQ(sceneNameAt_real(0), String());


  check::begin("scenes: isValidSceneName");

  // Valid names
  CHECK(isValidSceneName_real("a"));
  CHECK(isValidSceneName_real("z"));
  CHECK(isValidSceneName_real("0"));
  CHECK(isValidSceneName_real("9"));
  CHECK(isValidSceneName_real("-"));
  CHECK(isValidSceneName_real("_"));
  CHECK(isValidSceneName_real("a0-_"));
  CHECK(isValidSceneName_real("my-scene"));
  CHECK(isValidSceneName_real("my_scene"));
  CHECK(isValidSceneName_real("1234567890"));
  CHECK(isValidSceneName_real("a1b2c3d4e5"));

  // Boundary conditions (length)
  CHECK(!isValidSceneName_real("")); // Empty string
  CHECK(isValidSceneName_real("12345678901234567890")); // Exact max length (20)
  CHECK(!isValidSceneName_real("123456789012345678901")); // Exceeds max length (21)

  // Invalid characters
  CHECK(!isValidSceneName_real(" ")); // Space
  CHECK(!isValidSceneName_real("A")); // Uppercase
  CHECK(!isValidSceneName_real("my scene"));
  CHECK(!isValidSceneName_real("My-Scene"));
  CHECK(!isValidSceneName_real("my-scene!"));
  CHECK(!isValidSceneName_real("my@scene"));
  CHECK(!isValidSceneName_real("scene/1"));
}
