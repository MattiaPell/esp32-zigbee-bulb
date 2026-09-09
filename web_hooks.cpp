#include "web_hooks.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#include "config.h"
#include "debug_log.h"

namespace {

constexpr size_t QUEUE_CAP = 8;
constexpr const char *PREFS_NS = "hooks";

struct HookEvent {
  bool used = false;
  String body;                 // Serialized JSON payload
  char url[WEBHOOK_MAX_URL_LENGTH + 1] = "";  // Single-URL override (test)
  uint8_t urlIndex = 0;        // Next configured URL to try
  uint8_t attempts = 0;        // Failures for the current URL
  uint32_t nextTryMs = 0;
};

HookEvent queue[QUEUE_CAP];
size_t usedCount = 0;

char urls[WEBHOOK_MAX_URLS][WEBHOOK_MAX_URL_LENGTH + 1];
size_t urlCount = 0;

Preferences prefs;

uint32_t backoffMs(uint8_t attempt) {  // attempt 1..: 1 s, 5 s, 30 s
  switch (attempt) {
    case 1: return 1000;
    case 2: return 5000;
    default: return 30000;
  }
}

// Names, scene names and details are ASCII-restricted by construction; this
// only strips characters that would break the JSON string.
void jsonSafeAppend(String &out, const char *text) {
  for (const char *p = text; *p != '\0'; ++p) {
    char c = *p;
    if (c == '"' || c == '\\' || (unsigned char)c < 0x20) c = ' ';
    out += c;
  }
}

HookEvent *firstUsed() {
  for (size_t i = 0; i < QUEUE_CAP; ++i) {
    if (queue[i].used) return &queue[i];
  }
  return nullptr;
}

void advanceUrl(HookEvent &e) {
  ++e.urlIndex;
  e.attempts = 0;
  e.nextTryMs = 0;
  if (e.url[0] != '\0' || e.urlIndex >= webHookUrlCount()) {
    e.used = false;
    if (usedCount > 0) --usedCount;
  }
}

// One blocking POST; true on any HTTP response (delivery reached the server).
bool postEvent(const String &url, const String &body) {
  HTTPClient http;
  http.setConnectTimeout(WEBHOOK_CONNECT_TIMEOUT_MS);
  http.setTimeout(WEBHOOK_RESPONSE_TIMEOUT_MS);
  bool began = false;
  WiFiClient plain;
  WiFiClientSecure secure;
  if (url.startsWith("https://")) {
    secure.setInsecure();  // TLS without certificate verification (see README).
    began = http.begin(secure, url);
  } else {
    began = http.begin(plain, url);
  }
  if (!began) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  http.end();
  if (code <= 0) {
    debugLogPrintf("Webhooks: %s failed (%d)\n", url.c_str(), code);
    return false;
  }
  debugLogPrintf("Webhooks: %s -> %d\n", url.c_str(), code);
  return true;
}

}  // namespace

void webHooksBegin() {
  prefs.begin(PREFS_NS, false);
  urlCount = prefs.getUChar("count", 0);
  if (urlCount > WEBHOOK_MAX_URLS) urlCount = WEBHOOK_MAX_URLS;
  for (size_t i = 0; i < urlCount; ++i) {
    String url = prefs.getString(("u" + String(i)).c_str(), "");
    url.toCharArray(urls[i], sizeof(urls[i]));
  }
#ifdef WEBHOOK_URL_1
  if (urlCount == 0) {
    webHookUrlAdd(WEBHOOK_URL_1);  // First-boot seed from secrets.h.
  }
#endif
}

void webHooksTick() {
  if (usedCount == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;  // Events wait while offline.

  HookEvent *e = firstUsed();
  if (e == nullptr) return;
  if ((int32_t)(millis() - e->nextTryMs) < 0) return;

  String url;
  if (e->url[0] != '\0') {
    url = e->url;  // Test event: one URL only.
  } else if (e->urlIndex < urlCount) {
    url = urls[e->urlIndex];
  }
  if (url.length() == 0) {
    advanceUrl(*e);
    return;
  }

  if (postEvent(url, e->body)) {
    advanceUrl(*e);
    return;
  }
  ++e->attempts;
  if (e->attempts >= WEBHOOK_MAX_ATTEMPTS) {
    advanceUrl(*e);  // Give up on this URL, try the next one.
  } else {
    e->nextTryMs = millis() + backoffMs(e->attempts);
  }
}

bool webHookEvent(const char *event, const char *bulbId, const char *bulbName,
                  const char *detail) {
  for (size_t i = 0; i < QUEUE_CAP; ++i) {
    if (queue[i].used) continue;

    HookEvent &e = queue[i];
    e.used = true;
    e.body = "{\"event\":\"";
    jsonSafeAppend(e.body, event);
    e.body += "\",\"bulb\":\"";
    jsonSafeAppend(e.body, bulbId);
    e.body += "\",\"name\":\"";
    jsonSafeAppend(e.body, bulbName);
    e.body += "\",\"detail\":\"";
    jsonSafeAppend(e.body, detail);
    e.body += "\",\"uptime_s\":";
    e.body += millis() / 1000;
    e.body += ",\"version\":\"";
    e.body += FW_VERSION;
    e.body += "\"}";
    e.url[0] = '\0';
    e.urlIndex = 0;
    e.attempts = 0;
    e.nextTryMs = 0;
    ++usedCount;
    return true;
  }
  debugLogPrintln("Webhooks: queue full, event dropped");
  return false;
}

bool webHookTest(const String &url) {
  if (!isValidWebHookUrl(url)) return false;
  for (size_t i = 0; i < QUEUE_CAP; ++i) {
    if (queue[i].used) continue;
    HookEvent &e = queue[i];
    e.used = true;
    e.body = "{\"event\":\"test\",\"bulb\":\"\",\"name\":\"\",\"detail\":\"probe\",\"uptime_s\":";
    e.body += millis() / 1000;
    e.body += ",\"version\":\"";
    e.body += FW_VERSION;
    e.body += "\"}";
    url.toCharArray(e.url, sizeof(e.url));
    e.urlIndex = 0;
    e.attempts = 0;
    e.nextTryMs = 0;
    ++usedCount;
    return true;
  }
  return false;
}

size_t webHookUrlCount() {
  return urlCount;
}

String webHookUrlAt(size_t index) {
  if (index >= urlCount) return String();
  return String(urls[index]);
}

bool isValidWebHookUrl(const String &url) {
  if (url.length() == 0 || url.length() > WEBHOOK_MAX_URL_LENGTH) return false;
  if (!url.startsWith("http://") && !url.startsWith("https://")) return false;
  for (unsigned i = 0; i < url.length(); ++i) {
    char c = url[i];
    if (c <= 0x20 || c == '"' || c == '\\') return false;
  }
  return true;
}

bool webHookUrlAdd(const String &url) {
  if (!isValidWebHookUrl(url)) return false;
  for (size_t i = 0; i < urlCount; ++i) {
    if (url.equals(urls[i])) return true;  // Already configured.
  }
  if (urlCount >= WEBHOOK_MAX_URLS) return false;
  url.toCharArray(urls[urlCount], sizeof(urls[urlCount]));
  ++urlCount;

  prefs.begin(PREFS_NS, false);
  prefs.putString(("u" + String(urlCount - 1)).c_str(), url);
  prefs.putUChar("count", (uint8_t)urlCount);
  debugLogPrintf("Webhooks: URL added (%u stored)\n", (unsigned)urlCount);
  return true;
}

bool webHookUrlRemove(const String &url) {
  for (size_t i = 0; i < urlCount; ++i) {
    if (!url.equals(urls[i])) continue;
    for (size_t j = i; j + 1 < urlCount; ++j) {
      strlcpy(urls[j], urls[j + 1], sizeof(urls[j]));
    }
    --urlCount;

    prefs.begin(PREFS_NS, false);
    prefs.remove(("u" + String(urlCount)).c_str());
    prefs.putUChar("count", (uint8_t)urlCount);
    return true;
  }
  return false;
}
