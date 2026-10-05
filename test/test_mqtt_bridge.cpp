#include "check.h"
#include <Arduino.h>
#include <Preferences.h>

// Override definitions in check.h/shim to avoid multiple definition errors
#define millis millis_mqtt
#define debugLogPrintf debugLogPrintf_mqtt
#define debugLogPrintln debugLogPrintln_mqtt

uint32_t mock_millis_mqtt = 0;
uint32_t millis() { return mock_millis_mqtt; }
void debugLogPrintf(const char*, ...) {}
void debugLogPrintln(const char*) {}

class WiFiClient {
public:
    int write(const uint8_t*, size_t) { return 0; }
    int read() { return -1; }
    int read(uint8_t*, size_t) { return 0; }
    int available() { return 0; }
    void stop() {}
    bool connect(const char*, uint16_t) { return true; }
    bool connected() { return false; }
};

#define WL_CONNECTED 3
class WiFiClass_mqtt {
public:
    int status() { return WL_CONNECTED; }
};
WiFiClass_mqtt WiFi_mqtt_obj;
#define WiFi WiFi_mqtt_obj

// Provide config mocks
#define MQTT_HOST "127.0.0.1"
#define MQTT_PREFIX "testprefix"

// Mocks for string functions
#define String_remove_mock
#define String_endsWith_mock

// We mock some bulb registry & zigbee bulbs functions
#include "bulb_registry.h"
#include "zigbee_bulbs.h"
#include "parse_rgb_payload.h"

// Provide required mocks for dependencies

#define bulbIeeeHex bulbIeeeHex_mqtt
String bulbIeeeHex(const Bulb*) { return "0011223344556677"; }

#define registryFindByIeeeHex registryFindByIeeeHex_mqtt
Bulb* registryFindByIeeeHex(const String&) { return nullptr; }

#define registryGet registryGet_mqtt
Bulb* registryGet(size_t) { return nullptr; }

#define registryCount registryCount_mqtt
size_t registryCount() { return 0; }

// Zigbee mocks - wrap to avoid collision
#define bulbSendOff bulbSendOff_mqtt
#define bulbSendOn bulbSendOn_mqtt
#define bulbSendToggle bulbSendToggle_mqtt
#define bulbSendBrightness bulbSendBrightness_mqtt
#define bulbSendKelvin bulbSendKelvin_mqtt
#define bulbSendRgb bulbSendRgb_mqtt
void bulbSendOff(Bulb*) {}
void bulbSendOn(Bulb*) {}
void bulbSendToggle(Bulb*) {}
void bulbSendBrightness(Bulb*, uint8_t, uint16_t) {}
void bulbSendKelvin(Bulb*, int, uint16_t) {}
void bulbSendRgb(Bulb*, uint8_t, uint8_t, uint8_t, uint16_t) {}

// MQTT functions - wrap to avoid collision with config tests
#define mqttBegin mqttBegin_mqtt
#define mqttTick mqttTick_mqtt
#define mqttConnected mqttConnected_mqtt
#define mqttEnabledRuntime mqttEnabledRuntime_mqtt
#define mqttSetEnabled mqttSetEnabled_mqtt

// Extract functions to test using #define tricks or just including the file
// But since we have anonymous namespaces we need to include the file directly
#define handleMqttCommand handleMqttCommand_test

// Also rename these to avoid conflict with zigbee_bulbs.h
#define bulbSendAllOff bulbSendAllOff_mqtt
#define bulbSendAllOn bulbSendAllOn_mqtt
#define bulbSendAllBrightness bulbSendAllBrightness_mqtt
#define bulbSendAllKelvin bulbSendAllKelvin_mqtt
#define bulbSendAllRgb bulbSendAllRgb_mqtt
void bulbSendAllOff_mqtt() {}
void bulbSendAllOn_mqtt() {}
void bulbSendAllBrightness_mqtt(uint8_t, uint16_t) {}
void bulbSendAllKelvin_mqtt(int, uint16_t) {}
void bulbSendAllRgb_mqtt(uint8_t, uint8_t, uint8_t, uint16_t) {}

// Include the source to test
#include "../mqtt_bridge.cpp"

void testParseRgbPayload() {
  check::begin("parseRgbPayload");

  uint8_t r = 0, g = 0, b = 0;

  // 1. Happy path: valid RGB JSON payload
  CHECK(parseRgbPayload("{\"color\":{\"r\":255,\"g\":128,\"b\":64}}", r, g, b));
  CHECK_EQ(r, 255);
  CHECK_EQ(g, 128);
  CHECK_EQ(b, 64);

  // 2. Missing "color" key
  CHECK(!parseRgbPayload("{\"r\":255,\"g\":128,\"b\":64}", r, g, b));

  // 3. Missing brace after "color"
  CHECK(!parseRgbPayload("{\"color\":\"red\"}", r, g, b));

  // 4. Missing one of the components
  CHECK(!parseRgbPayload("{\"color\":{\"r\":255,\"g\":128}}", r, g, b));

  // 5. Out of bounds components (should be constrained)
  CHECK(parseRgbPayload("{\"color\":{\"r\":300,\"g\":-10,\"b\":64}}", r, g, b));
  CHECK_EQ(r, 255);
  CHECK_EQ(g, 0);
  CHECK_EQ(b, 64);

  // 6. Valid components with surrounding whitespace
  CHECK(parseRgbPayload("{\"color\": { \"r\": 10, \"g\": 20, \"b\": 30 } }", r, g, b));
  CHECK_EQ(r, 10);
  CHECK_EQ(g, 20);
  CHECK_EQ(b, 30);

  // 7. Embedded within a larger MQTT payload
  CHECK(parseRgbPayload("{\"state\":\"ON\",\"brightness\":128,\"color\":{\"r\":100,\"g\":200,\"b\":50}}", r, g, b));
  CHECK_EQ(r, 100);
  CHECK_EQ(g, 200);
  CHECK_EQ(b, 50);
}

void testTopicGeneration() {
  check::begin("mqtt topics");

  Bulb b;
  // Initialize standard IEEE addr manually
  b.ieee[0] = 0x00;
  b.ieee[1] = 0x11;
  b.ieee[2] = 0x22;
  b.ieee[3] = 0x33;
  b.ieee[4] = 0x44;
  b.ieee[5] = 0x55;
  b.ieee[6] = 0x66;
  b.ieee[7] = 0x77;

  // Instead of matching exactly testprefix-myhost, we use the value returned by storedHostname()
  // which might be "esp32" from default config.h
  String bridge = String("testprefix-") + storedHostname();
  CHECK_EQ(bridgeUid(), bridge);
  CHECK_EQ(bulbUid(&b), bridge + "-0011223344556677");
  CHECK_EQ(topicFor("/bridge"), "testprefix/bridge");
  CHECK_EQ(topicForBulb(&b, "/state"), "testprefix/0011223344556677/state");
}

void testStateJson() {
  check::begin("mqtt state json");

  Bulb b;

  // Test white mode
  b.state.mode = BulbColorMode::White;
  b.state.power = true;
  b.state.level = 200;
  b.state.kelvin = 2500;

  String jsonStr = stateJson(&b);
  CHECK_EQ(jsonStr, "{\"state\":\"ON\",\"brightness\":200,\"color_mode\":\"color_temp\",\"color_temp\":400}");

  // Test rgb mode
  b.state.mode = BulbColorMode::Rgb;
  b.state.power = false;
  b.state.level = 100;
  b.state.red = 255;
  b.state.green = 128;
  b.state.blue = 64;

  jsonStr = stateJson(&b);
  CHECK_EQ(jsonStr, "{\"state\":\"OFF\",\"brightness\":100,\"color_mode\":\"rgb\",\"color\":{\"r\":255,\"g\":128,\"b\":64}}");
}

void runMqttBridgeTests() {
  testParseRgbPayload();
  testTopicGeneration();
  testStateJson();
}
