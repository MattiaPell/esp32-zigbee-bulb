#include "check.h"

#include <cstdint>

// Rename the symbols this translation unit defines so they don't collide with
// the other suites: test_light_timers.cpp and test_bulb_registry.cpp each own
// their own millis()/debugLog* variants, and everything is linked into a
// single host binary.
#define millis my_millis
#define debugLogPrintf my_debugLogPrintf
#define debugLogPrintln my_debugLogPrintln

uint32_t my_mock_millis_web_hooks = 0;
uint32_t my_millis() { return my_mock_millis_web_hooks; }
void my_debugLogPrintf(const char *, ...) {}
void my_debugLogPrintln(const char *) {}

#define pdPASS 1
void vTaskDelete(void *) {}
int xTaskCreate(void (*)(void *), const char *, uint32_t, void *, uint32_t, void *) {
  return pdPASS;
}

// The shims for these headers are empty, so the classes web_hooks.cpp touches
// are declared here before it is included -- only with the surface it uses.

class WiFiClient {
public:
  WiFiClient() {}
};

struct sslclient_context;  // opaque: web_hooks.cpp only passes the pointer on

class NetworkClientSecure : public WiFiClient {
public:
  NetworkClientSecure() {}

protected:
  // Mirrors the std::shared_ptr<sslclient_context> member of the real class.
  struct SslContext {
    sslclient_context *ctx = nullptr;
    sslclient_context *get() const { return ctx; }
  } sslclient;
  bool _use_ca_bundle = false;
};

// Stand-in for ssl_client.h's hook (see BundleVerifyingClient in web_hooks.cpp).
void attach_ssl_certificate_bundle(sslclient_context *, bool) {}

class HTTPClient {
public:
  void setConnectTimeout(uint32_t) {}
  void setTimeout(uint32_t) {}
  bool begin(WiFiClient &, const String &) { return true; }
  void addHeader(const String &, const String &) {}
  int POST(const String &) { return 200; }
  void end() {}
};

#define WL_CONNECTED 3
class WiFiClass {
public:
  int status() { return WL_CONNECTED; }
};
WiFiClass WiFi;

// test_light_timers.cpp defines its own webHookEvent mock: rename ours.
#define webHookEvent webHookEvent_web_hooks

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
  CHECK_EQ(maxUrl.length(), (unsigned)WEBHOOK_MAX_URL_LENGTH);
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
