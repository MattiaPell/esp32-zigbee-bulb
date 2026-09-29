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
}
