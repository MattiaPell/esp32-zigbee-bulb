#include "check.h"
#include <string>
#include <vector>
#include <string.h>
#include <cstdint>

#include "WString.h"

// We must include bulb_registry.h properly so we get Bulb struct.
#define HOST_TEST 1
#include "../bulb_registry.h"
#include "../config.h"

// Mocks

// Mock Serial
static struct MockSerialSerialConsole {
  bool available() { return input.length() > 0; }
  String readStringUntil(char c) {
    size_t pos = input.indexOf(c);
    if (pos == (size_t)-1) {
      String ret = input;
      input = "";
      return ret;
    }
    String ret = input.substring(0, pos);
    input = input.substring(pos + 1);
    return ret;
  }
  void setTimeout(long /*timeout*/) {}
  void println(const char* /*str*/) {}

  String input;
} SerialConsole;

#define Serial SerialConsole

// Mock registry
static size_t mock_serial_registry_count = 0;
static Bulb mock_serial_bulbs[10];

#define registryCount mock_serial_registryCount
static size_t mock_serial_registryCount() { return mock_serial_registry_count; }

#define registryGet mock_serial_registryGet
static const Bulb* mock_serial_registryGet(size_t index) {
  if (index < mock_serial_registry_count) return &mock_serial_bulbs[index];
  return nullptr;
}

// Mock Bulb Commands
static int mock_bulb_on_calls = 0;
static int mock_bulb_off_calls = 0;
static const Bulb* mock_last_bulb_commanded = nullptr;

#define bulbSendOn mock_serial_bulbSendOn
static void mock_serial_bulbSendOn(const Bulb* bulb) { mock_bulb_on_calls++; mock_last_bulb_commanded = bulb; }

#define bulbSendOff mock_serial_bulbSendOff
static void mock_serial_bulbSendOff(const Bulb* bulb) { mock_bulb_off_calls++; mock_last_bulb_commanded = bulb; }

static int mock_bulb_all_off_calls = 0;
#define bulbSendAllOff mock_serial_bulbSendAllOff
static int mock_serial_bulbSendAllOff() { mock_bulb_all_off_calls++; return 0; }

// Mock Zigbee
static int mock_zigbee_pairing_calls = 0;
static int mock_zigbee_pairing_seconds = 0;
#define zigbeeOpenPairing mock_serial_zigbeeOpenPairing
static void mock_serial_zigbeeOpenPairing(uint8_t seconds) { mock_zigbee_pairing_calls++; mock_zigbee_pairing_seconds = seconds; }

static int mock_zigbee_factory_reset_calls = 0;
static bool mock_zigbee_factory_reset_arg = false;

static struct MockZigbeeSerialConsole {
  void factoryReset(bool b) { mock_zigbee_factory_reset_calls++; mock_zigbee_factory_reset_arg = b; }
} ZigbeeConsole;

#define Zigbee ZigbeeConsole

// Mock WiFi
struct MockIPAddressSerialConsole {
  String toString() const { return "192.168.1.100"; }
};
static struct MockWiFiSerialConsole {
  int status() { return 3; /* WL_CONNECTED */ }
  MockIPAddressSerialConsole localIP() { return MockIPAddressSerialConsole(); }
  int RSSI() { return -50; }
} WiFiConsole;

#define WiFi WiFiConsole
#ifndef WL_CONNECTED
#define WL_CONNECTED 3
#endif

// Mock WebServer
static bool mock_web_set_hostname_result = true;
static String mock_web_set_hostname_arg = "";
#define webSetHostname mock_serial_webSetHostname
static bool mock_serial_webSetHostname(const String& name) { mock_web_set_hostname_arg = name; return mock_web_set_hostname_result; }

// Mock OTA
#define otaRunningSlot mock_serial_otaRunningSlot
static const char* mock_serial_otaRunningSlot() { return "app0"; }
#define otaPendingVerify mock_serial_otaPendingVerify
static bool mock_serial_otaPendingVerify() { return false; }

// Mock DebugLog
#define debugLogPrintln debugLogPrintln_serial
static void debugLogPrintln(const char* /*text*/) {}
#define debugLogPrintf debugLogPrintf_serial
static void debugLogPrintf(const char* /*fmt*/, ...) {}

// Mock ESP
static struct MockESPSerialConsole {
  int getFreeHeap() { return 100000; }
  void restart() { mock_esp_restart_calls++; }
  int mock_esp_restart_calls = 0;
} ESPConsole;

#define ESP ESPConsole

#define zigbeeBoundDeviceCount mock_serial_zigbeeBoundDeviceCount
static size_t mock_serial_zigbeeBoundDeviceCount() { return 0; }

// Include target source file
#include "../serial_console.cpp"

void runSerialConsoleTests() {
  check::begin("serial_console");

  // Reset mocks
  mock_serial_registry_count = 2;
  strcpy(mock_serial_bulbs[0].name, "Bulb 1");
  strcpy(mock_serial_bulbs[1].name, "Bulb 2");
  mock_serial_bulbs[0].online = true;
  mock_serial_bulbs[0].shortAddr = 0x1234;
  mock_serial_bulbs[0].endpoint = 1;

  mock_bulb_on_calls = 0;
  mock_bulb_off_calls = 0;
  mock_bulb_all_off_calls = 0;
  mock_zigbee_pairing_calls = 0;
  mock_web_set_hostname_result = true;
  mock_web_set_hostname_arg = "";
  ESP.mock_esp_restart_calls = 0;
  mock_zigbee_factory_reset_calls = 0;

  // Test "help", "list", "status", "version"
  Serial.input = "help\nlist\nstatus\nversion\n";
  serialConsoleTick();
  serialConsoleTick();
  serialConsoleTick();
  serialConsoleTick();

  // Test "on"
  Serial.input = "on 1\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_on_calls, 1);
  CHECK_EQ(mock_last_bulb_commanded == &mock_serial_bulbs[0], true);

  Serial.input = "on 2\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_on_calls, 2);
  CHECK_EQ(mock_last_bulb_commanded == &mock_serial_bulbs[1], true);

  // Test "off"
  Serial.input = "off 1\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_off_calls, 1);
  CHECK_EQ(mock_last_bulb_commanded == &mock_serial_bulbs[0], true);

  // Test out-of-bounds "on"
  Serial.input = "on 3\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_on_calls, 2); // Unchanged

  Serial.input = "on 0\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_on_calls, 2); // Unchanged

  // Test "all off"
  Serial.input = "all off\n";
  serialConsoleTick();
  CHECK_EQ(mock_bulb_all_off_calls, 1);

  // Test "pair"
  Serial.input = "pair\n";
  serialConsoleTick();
  CHECK_EQ(mock_zigbee_pairing_calls, 1);
  CHECK_EQ(mock_zigbee_pairing_seconds > 0, true);

  // Test "host"
  Serial.input = "host my-new-host\n";
  serialConsoleTick();
  CHECK_EQ(mock_web_set_hostname_arg, String("my-new-host"));

  // Test "reboot"
  Serial.input = "reboot\n";
  serialConsoleTick();
  CHECK_EQ(ESP.mock_esp_restart_calls, 1);

  // Test "reset"
  Serial.input = "reset\n";
  serialConsoleTick();
  CHECK_EQ(mock_zigbee_factory_reset_calls, 1);
  CHECK_EQ(mock_zigbee_factory_reset_arg, true);
}
