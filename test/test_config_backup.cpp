#include "check.h"
#include <cstdint>

#include "bulb_registry.h"
#include "config.h"
#include "json_lite.h"
#include "mqtt_bridge.h"
#include "scenes.h"
#include "web_hooks.h"
#include "web_server.h"

String sceneNameAt(size_t /*i*/) { return ""; }
String sceneEntryAt(size_t /*i*/) { return ""; }
Bulb* registryGet(size_t /*i*/) { return nullptr; }
size_t registryCount() { return 0; }
Bulb* registryAdd(const esp_zb_ieee_addr_t /*ieee*/) { return nullptr; }
void registryRename(Bulb* /*b*/, const char* /*name*/) {}
void registryMarkDirty() {}
void registryFlush() {}
bool webHookUrlAdd(const String& /*url*/) { return false; }
size_t scenesCount() { return 0; }
String webHookUrlAt(size_t /*i*/) { return ""; }
size_t webHookUrlCount() { return 0; }
bool mqttEnabledRuntime() { return false; }
void mqttSetEnabled(bool /*e*/) {}
bool webSetHostname(const String& /*h*/) { return true; }

bool deserializeBulbState(BulbState& /*state*/, const String& /*str*/) { return false; }
String serializeBulbState(const BulbState& /*state*/) { return ""; }
bool isValidSceneName(const String& /*name*/) { return false; }
bool sceneImport(const String& /*name*/, const String& /*data*/) { return false; }

// Include the source directly to test its static/internal functions
#include "../config_backup.cpp"

namespace {
void testIsValidIeeeHex() {
    check::begin("isValidIeeeHex");
    CHECK(isValidIeeeHex("0123456789abcdef"));
    CHECK(isValidIeeeHex("0123456789ABCDEF"));
    CHECK(isValidIeeeHex("aAbBcCdDeEfF0123"));
    CHECK(!isValidIeeeHex("0123456789abcdeg"));
    CHECK(!isValidIeeeHex("0123456789abcde"));
    CHECK(!isValidIeeeHex("0123456789abcdef0"));
    CHECK(!isValidIeeeHex(" 123456789abcdef"));
    CHECK(!isValidIeeeHex("0123456789abcde-"));
}
} // namespace

void runConfigBackupTests() {
    testIsValidIeeeHex();
}
