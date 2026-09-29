#include "check.h"
#include <cstdint>
#include <string.h>

#include "WString.h"
#include "config.h"
#include "Preferences.h"

// Stubs for testing web_hooks.cpp
class WiFiClient {
public:
  WiFiClient() {}
};

class WiFiClientSecure : public WiFiClient {
public:
  WiFiClientSecure() {}
  void setInsecure() {}
};

class HTTPClient {
public:
  HTTPClient() {}
  void setConnectTimeout(uint32_t) {}
  void setTimeout(uint32_t) {}
  bool begin(WiFiClient&, const String&) { return true; }
  void addHeader(const char*, const char*) {}
  int POST(const String&) { return 200; }
  void end() {}
};

class WiFiClass {
public:
  int status() { return 3; } // WL_CONNECTED = 3
};
WiFiClass WiFi;
#define WL_CONNECTED 3

#define pdPASS 1
typedef void* TaskHandle_t;
int xTaskCreate(void (*)(void*), const char*, uint32_t, void*, uint32_t, TaskHandle_t*) { return pdPASS; }
void vTaskDelete(TaskHandle_t) {}

uint32_t millis();

#define FW_VERSION "1.0.0"

// Rename the real webHookEvent to avoid conflicting with the mock in test_light_timers.cpp
#define webHookEvent real_webHookEvent

#include "../web_hooks.cpp"

#undef webHookEvent

void runWebHooksTests() {
  check::begin("web hooks");

  // Happy paths
  CHECK(isValidWebHookUrl("http://example.com"));
  CHECK(isValidWebHookUrl("https://example.com/webhook"));
  CHECK(isValidWebHookUrl("http://192.168.1.1/hook"));

  // URL exactly max chars (max length)
  String maxUrl = "http://example.com/";
  while (maxUrl.length() < WEBHOOK_MAX_URL_LENGTH) {
    maxUrl += "a";
  }
  CHECK_EQ(maxUrl.length(), WEBHOOK_MAX_URL_LENGTH);
  CHECK(isValidWebHookUrl(maxUrl));

  // URL too long
  String tooLongUrl = maxUrl + "a";
  CHECK(!isValidWebHookUrl(tooLongUrl));

  // Length < 10
  CHECK(!isValidWebHookUrl("http://a"));
  CHECK(!isValidWebHookUrl(""));

  // Invalid prefix
  CHECK(!isValidWebHookUrl("ftp://example.com"));
  CHECK(!isValidWebHookUrl("example.com"));
  CHECK(!isValidWebHookUrl("HTTP://example.com"));

  // Spaces and invalid characters
  CHECK(!isValidWebHookUrl("http://example.com/a b"));
  CHECK(!isValidWebHookUrl("http://example.com/\""));
  CHECK(!isValidWebHookUrl("http://example.com/\\"));

  // Control characters
  String controlCharUrl = "http://example.com/";
  controlCharUrl += (char)0x1F;
  CHECK(!isValidWebHookUrl(controlCharUrl));
}
