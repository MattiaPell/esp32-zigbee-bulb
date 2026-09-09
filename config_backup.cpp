#include "config_backup.h"

#include "bulb_registry.h"
#include "config.h"
#include "json_lite.h"
#include "mqtt_bridge.h"
#include "scenes.h"
#include "web_hooks.h"
#include "web_server.h"

namespace {

constexpr size_t MAX_RESTORE_BULBS = MAX_BULBS;
constexpr size_t MAX_RESTORE_SCENES = MAX_SCENES;

// --- JSON array walkers (objects or plain strings) ---------------------------

// Skips a quoted string starting at body[pos] ('"'); returns the position
// after the closing quote.
size_t skipJsonString(const String &body, size_t pos) {
  ++pos;  // Opening quote.
  while (pos < body.length()) {
    if (body[pos] == '\\') {
      pos += 2;
      continue;
    }
    if (body[pos] == '"') return pos + 1;
    ++pos;
  }
  return pos;
}

// Extracts the top-level {...} objects from the "key":[ ... ] array.
size_t extractObjectArray(const String &body, const char *key, String *out,
                          size_t cap) {
  const size_t at = findJsonKey(body, key);
  if (at == (size_t)-1 || at >= body.length() || body[at] != '[') return 0;
  size_t pos = at + 1;
  size_t n = 0;
  while (pos < body.length() && n < cap) {
    while (pos < body.length() && (isspace((unsigned char)body[pos]) || body[pos] == ',')) {
      ++pos;
    }
    if (pos >= body.length() || body[pos] != '{') break;
    const size_t start = pos;
    int depth = 0;
    while (pos < body.length()) {
      const char c = body[pos];
      if (c == '"') {
        pos = skipJsonString(body, pos);
        continue;
      }
      if (c == '{') ++depth;
      if (c == '}') {
        --depth;
        if (depth == 0) {
          ++pos;
          break;
        }
      }
      ++pos;
    }
    out[n++] = body.substring(start, pos);
  }
  return n;
}

// Extracts the quoted strings from the "key":[ ... ] array.
size_t extractStringArray(const String &body, const char *key, String *out,
                          size_t cap) {
  const size_t at = findJsonKey(body, key);
  if (at == (size_t)-1 || at >= body.length() || body[at] != '[') return 0;
  size_t pos = at + 1;
  size_t n = 0;
  while (pos < body.length() && n < cap) {
    while (pos < body.length() && (isspace((unsigned char)body[pos]) || body[pos] == ',')) {
      ++pos;
    }
    if (pos >= body.length() || body[pos] != '"') break;
    const size_t start = pos;
    pos = skipJsonString(body, pos);
    if (pos > start + 2) out[n++] = body.substring(start + 1, pos - 1);
  }
  return n;
}

// The value object of "key":{...} ("" if missing).
String extractObjectValue(const String &body, const char *key) {
  const size_t at = findJsonKey(body, key);
  if (at == (size_t)-1 || at >= body.length() || body[at] != '{') return String();
  const size_t start = at;
  int depth = 0;
  size_t pos = at;
  while (pos < body.length()) {
    const char c = body[pos];
    if (c == '"') {
      pos = skipJsonString(body, pos);
      continue;
    }
    if (c == '{') ++depth;
    if (c == '}') {
      --depth;
      if (depth == 0) {
        ++pos;
        break;
      }
    }
    ++pos;
  }
  return body.substring(start, pos);
}

bool isValidIeeeHex(const String &hex) {
  if (hex.length() != 16) return false;
  for (unsigned i = 0; i < hex.length(); ++i) {
    const char c = hex[i];
    const bool digit = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                       (c >= 'A' && c <= 'F');
    if (!digit) return false;
  }
  return true;
}

void appendBulbJson(const Bulb *b, String &j) {
  j += "{\"ieee\":\"";
  j += bulbIeeeHex(b);
  j += "\",\"name\":\"";
  j += b->name;
  j += "\",\"state\":\"";
  j += serializeBulbState(b->state);
  j += "\"}";
}

void appendSceneJson(size_t index, String &j) {
  j += "{\"name\":\"";
  j += sceneNameAt(index);
  j += "\",\"bulbs\":[";
  const String entry = sceneEntryAt(index);
  int start = 0;
  bool first = true;
  while (start < (int)entry.length()) {
    const int bar = entry.indexOf('|', start);
    const String item = bar < 0 ? entry.substring(start) : entry.substring(start, bar);
    start = bar < 0 ? (int)entry.length() : bar + 1;
    const int semi = item.indexOf(';');
    if (semi <= 0) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"ieee\":\"";
    j += item.substring(0, semi);
    j += "\",\"state\":\"";
    j += item.substring(semi + 1);
    j += "\"}";
  }
  j += "]}";
}

}  // namespace

String configBackupJson() {
  String j;
  j.reserve(2048);
  j += "{\"version\":1";
  j += ",\"hostname\":\"";
  j += storedHostname();
  j += "\",\"bulbs\":[";
  for (size_t i = 0; i < registryCount(); ++i) {
    if (i > 0) j += ",";
    appendBulbJson(registryGet(i), j);
  }
  j += "],\"scenes\":[";
  const size_t sceneTotal = scenesCount();
  for (size_t i = 0; i < sceneTotal; ++i) {
    if (i > 0) j += ",";
    appendSceneJson(i, j);
  }
  j += "],\"hooks\":{\"urls\":[";
  for (size_t i = 0; i < webHookUrlCount(); ++i) {
    if (i > 0) j += ",";
    j += "\"";
    j += webHookUrlAt(i);
    j += "\"";
  }
  j += "]},\"mqtt\":{\"enabled\":";
  j += mqttEnabledRuntime() ? "true" : "false";
  j += "}}";
  return j;
}

bool configRestoreJson(const String &body, String &summary) {
  long version = 0;
  if (!jsonGetInt(body, "version", version) || version != 1) {
    return false;
  }

  size_t bulbsRestored = 0, bulbsSkipped = 0;
  String bulbObjs[MAX_RESTORE_BULBS];
  const size_t bulbN = extractObjectArray(body, "bulbs", bulbObjs, MAX_RESTORE_BULBS);
  for (size_t i = 0; i < bulbN; ++i) {
    String ieeeHex, name, stateStr;
    if (!jsonGetString(bulbObjs[i], "ieee", ieeeHex) ||
        !jsonGetString(bulbObjs[i], "state", stateStr) ||
        !isValidIeeeHex(ieeeHex)) {
      ++bulbsSkipped;
      continue;
    }
    BulbState wanted;
    if (!deserializeBulbState(wanted, stateStr)) {
      ++bulbsSkipped;
      continue;
    }

    // Locate by IEEE hex (little-endian binary), matching the API id.
    Bulb *b = nullptr;
    for (size_t k = 0; k < registryCount(); ++k) {
      if (bulbIeeeHex(registryGet(k)).equalsIgnoreCase(ieeeHex)) {
        b = registryGet(k);
        break;
      }
    }
    if (b == nullptr) {
      esp_zb_ieee_addr_t ieee = {0};
      for (int k = 0; k < 8; ++k) {
        const char hi = ieeeHex[15 - 2 * k];
        const char lo = ieeeHex[14 - 2 * k];
        const uint8_t hiV = hi <= '9' ? (uint8_t)(hi - '0') : (uint8_t)(tolower(hi) - 'a' + 10);
        const uint8_t loV = lo <= '9' ? (uint8_t)(lo - '0') : (uint8_t)(tolower(lo) - 'a' + 10);
        ieee[k] = (uint8_t)((hiV << 4) | loV);
      }
      b = registryAdd(ieee);  // nullptr when the registry is full.
    }
    if (b == nullptr) {
      ++bulbsSkipped;
      continue;
    }
    if (jsonGetString(bulbObjs[i], "name", name) && name.length() > 0) {
      registryRename(b, name.c_str());
    }
    b->state = wanted;
    registryMarkDirty();
    ++bulbsRestored;
  }
  registryFlush();

  size_t scenesRestored = 0;
  String sceneObjs[MAX_RESTORE_SCENES];
  const size_t sceneN = extractObjectArray(body, "scenes", sceneObjs, MAX_RESTORE_SCENES);
  for (size_t i = 0; i < sceneN; ++i) {
    String name;
    if (!jsonGetString(sceneObjs[i], "name", name) || !isValidSceneName(name)) continue;

    String bulbStrs[MAX_BULBS];
    const size_t n = extractObjectArray(sceneObjs[i], "bulbs", bulbStrs, MAX_BULBS);
    String data;
    data.reserve(64 * n);
    for (size_t k = 0; k < n; ++k) {
      String ieeeHex, stateStr;
      if (!jsonGetString(bulbStrs[k], "ieee", ieeeHex) ||
          !jsonGetString(bulbStrs[k], "state", stateStr) ||
          !isValidIeeeHex(ieeeHex)) {
        continue;
      }
      if (data.length() > 0) data += "|";
      data += ieeeHex;
      data += ";";
      data += stateStr;
    }
    if (data.length() == 0) continue;
    if (sceneImport(name, data)) ++scenesRestored;
  }

  size_t hooksRestored = 0;
  const String hooksObj = extractObjectValue(body, "hooks");
  if (hooksObj.length() > 0) {
    String hookUrls[WEBHOOK_MAX_URLS];
    const size_t n = extractStringArray(hooksObj, "urls", hookUrls, WEBHOOK_MAX_URLS);
    for (size_t i = 0; i < n; ++i) {
      if (webHookUrlAdd(hookUrls[i])) ++hooksRestored;
    }
  }

  const String mqttObj = extractObjectValue(body, "mqtt");
  if (mqttObj.length() > 0) {
    bool enabled = false;
    if (jsonGetBool(mqttObj, "enabled", enabled)) mqttSetEnabled(enabled);
  }

  String hostname;
  if (jsonGetString(body, "hostname", hostname)) {
    webSetHostname(hostname);  // Validation inside; ignored when invalid.
  }

  summary = "{\"bulbs\":" + String(bulbsRestored) +
            ",\"bulbs_skipped\":" + String(bulbsSkipped) +
            ",\"scenes\":" + String(scenesRestored) +
            ",\"hooks\":" + String(hooksRestored) + "}";
  return true;
}
