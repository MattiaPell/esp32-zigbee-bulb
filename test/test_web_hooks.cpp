#include "check.h"
#include <cstdint>

// Override these specific function names so they don't clash with test_light_timers.cpp during linking.
// We only need them to compile web_hooks.cpp.
#define millis my_millis
#define debugLogPrintf my_debugLogPrintf
#define debugLogPrintln my_debugLogPrintln

uint32_t my_mock_millis_web_hooks = 0;
uint32_t my_millis() { return my_mock_millis_web_hooks; }
void my_debugLogPrintf(const char *, ...) {}
void my_debugLogPrintln(const char *) {}

#define pdPASS 1

void vTaskDelete(void *) {}
int xTaskCreate(void (*)(void*), const char*, uint32_t, void*, uint32_t, void*) { return pdPASS; }

// WiFi and HTTP stubs
class WiFiClient {};
class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
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
    void setConnectTimeout(int) {}
    void setTimeout(int) {}
    bool begin(WiFiClient&, const String&) { return true; }
    void addHeader(const String&, const String&) {}
    int POST(const String&) { return 200; }
    void end() {}
};

#define WL_CONNECTED 1
class WiFiClass {
public:
    int status() { return WL_CONNECTED; }
};
WiFiClass WiFi;

// We need to rename webHookEvent inside web_hooks.cpp to avoid conflict with test_light_timers.cpp
#define webHookEvent webHookEvent_real

// Temporarily suppress unused parameter warning in web_hooks.cpp for the mock build
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../web_hooks.cpp"
#pragma GCC diagnostic pop
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
  check::begin("jsonSafeAppend");
  String out;

  // Happy path
  out = "";
  jsonSafeAppend(out, "hello world");
  CHECK_EQ(out, "hello world");

  // Quotes are escaped
  out = "";
  jsonSafeAppend(out, "a\"b");
  CHECK_EQ(out, "a\\\"b");

  // Backslashes are escaped
  out = "";
  jsonSafeAppend(out, "a\\b");
  CHECK_EQ(out, "a\\\\b");

  // Control characters are skipped (neither escaped nor appended, just skipped)
  out = "";
  jsonSafeAppend(out, "a\nb\r\tc");
  CHECK_EQ(out, "abc");

  // Original out is kept
  out = "start_";
  jsonSafeAppend(out, "end");
  CHECK_EQ(out, "start_end");
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
