#include "check.h"

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

size_t registryCount() { return 0; }
Bulb* registryGet(size_t) { return nullptr; }
// bulbIeeeHex and registryFindByIeee are defined in test_light_timers.cpp!
String serializeBulbState(const BulbState&) { return ""; }
bool deserializeBulbState(BulbState&, const String&) { return false; }
void registryRename(Bulb*, const char*) {}
void registryMarkDirty() {}
void registryFlush() {}
Bulb* registryAdd(esp_zb_ieee_addr_t) { return nullptr; }

size_t scenesCount() { return 0; }
String sceneNameAt(size_t) { return ""; }
String sceneEntryAt(size_t) { return ""; }
bool isValidSceneName(const String&) { return false; }
bool sceneImport(const String&, const String&) { return false; }

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
