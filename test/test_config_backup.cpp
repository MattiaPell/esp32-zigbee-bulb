#include "check.h"
#include <cstdint>

// We need to compile config_backup.cpp to test its internal function skipJsonString.
// Since test/run.sh compiles all test/*.cpp files together, we need to be careful
// with mock definitions to avoid multiple definition errors (especially with
// test_light_timers.cpp which already defines some mocks).
// For test_config_backup, we can just declare the few mocks needed to compile
// config_backup.cpp, making sure not to redefine things already in other test files.
// Wait, actually config_backup.cpp uses functions like extractObjectArray, skipJsonString
// which are in the anonymous namespace in config_backup.cpp.

// The simplest way to test a function inside an anonymous namespace is to include
// the cpp file directly. But we must stub any missing dependencies that aren't provided
// by other test files.

#include "bulb_registry.h"
#include "config.h"
#include "json_lite.h"
#include "mqtt_bridge.h"
#include "scenes.h"
#include "web_hooks.h"
#include "web_server.h"

// Stubs for config_backup.cpp that are not in test_light_timers.cpp
bool mqttEnabledRuntime() { return false; }
void mqttSetEnabled(bool) {}
bool webSetHostname(const String &) { return true; }

#define registryCount mock_registryCount_cfg
#define registryGet mock_registryGet_cfg
#define serializeBulbState mock_serializeBulbState_cfg
#define deserializeBulbState mock_deserializeBulbState_cfg
#define registryRename mock_registryRename_cfg
#define registryMarkDirty mock_registryMarkDirty_cfg
#define registryFlush mock_registryFlush_cfg
#define registryAdd mock_registryAdd_cfg

#define scenesCount mock_scenesCount_cfg
#define sceneNameAt mock_sceneNameAt_cfg
#define sceneEntryAt mock_sceneEntryAt_cfg
#define isValidSceneName mock_isValidSceneName_cfg
#define sceneImport mock_sceneImport_cfg

size_t mock_registryCount_cfg() { return 0; }
Bulb* mock_registryGet_cfg(size_t) { return nullptr; }
String mock_serializeBulbState_cfg(const BulbState&) { return ""; }
bool mock_deserializeBulbState_cfg(BulbState&, const String&) { return false; }
void mock_registryRename_cfg(Bulb*, const char*) {}
void mock_registryMarkDirty_cfg() {}
void mock_registryFlush_cfg() {}
Bulb* mock_registryAdd_cfg(esp_zb_ieee_addr_t) { return nullptr; }

size_t mock_scenesCount_cfg() { return 0; }
String mock_sceneNameAt_cfg(size_t) { return ""; }
String mock_sceneEntryAt_cfg(size_t) { return ""; }
bool mock_isValidSceneName_cfg(const String&) { return false; }
bool mock_sceneImport_cfg(const String&, const String&) { return false; }

size_t webHookUrlCount() { return 0; }
String webHookUrlAt(size_t) { return ""; }
bool webHookUrlAdd(const String&) { return false; }

#include "../config_backup.cpp"

namespace {
void testSkipJsonString() {
  check::begin("config_backup: skipJsonString");

  String body = "\"hello\"";
  CHECK_EQ(skipJsonString(body, 0), (size_t)7);

  body = "\"he\\\"llo\"";
  CHECK_EQ(skipJsonString(body, 0), (size_t)9);

  body = "\"\"";
  CHECK_EQ(skipJsonString(body, 0), (size_t)2);

  body = "\"hello\\\\\"";
  CHECK_EQ(skipJsonString(body, 0), (size_t)9);

  // Missing closing quote
  body = "\"hello";
  CHECK_EQ(skipJsonString(body, 0), (size_t)6);

  // Starting not at 0
  body = "foo:\"bar\",baz";
  CHECK_EQ(skipJsonString(body, 4), (size_t)9);
}
} // namespace

void runConfigBackupTests() {
  testSkipJsonString();
}
