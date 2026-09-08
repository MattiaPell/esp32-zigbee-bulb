#include "web_server.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and enter your Wi-Fi credentials"
#endif

#include "bulb_registry.h"
#include "config.h"
#include "scenes.h"
#include "web_page.h"
#include "zigbee_bulbs.h"

namespace {

WebServer server(WEB_PORT);
bool mdnsUp = false;
uint32_t lastWifiRetryMs = 0;
const char *FW_VERSION = "1.0.0";

Preferences webPrefs;

String hostnameValue = DEFAULT_HOSTNAME;

// --- JSON helpers (targeted: the API speaks only this dialect) --------------

void sendJson(int code, const String &payload) {
  server.send(code, "application/json", payload);
}

void sendJsonError(int code, const char *message) {
  server.send(code, "application/json", String("{\"error\":\"") + message + "\"}");
}

size_t findJsonKey(const String &body, const char *key) {
  // Find "key", skip whitespace, expect ':'; tolerant to spaced-out JSON.
  String needle = "\"";
  needle += key;
  needle += "\"";
  int at = body.indexOf(needle);
  if (at < 0) return (size_t)-1;
  size_t pos = at + needle.length();
  while (pos < body.length() && isspace((unsigned char)body[pos])) ++pos;
  if (pos >= body.length() || body[pos] != ':') return (size_t)-1;
  ++pos;
  while (pos < body.length() && isspace((unsigned char)body[pos])) ++pos;
  return pos;
}

bool jsonGetBool(const String &body, const char *key, bool &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1) return false;
  if (body.indexOf("true", at) == (int)at) {
    out = true;
    return true;
  }
  if (body.indexOf("false", at) == (int)at) {
    out = false;
    return true;
  }
  return false;
}

bool jsonGetInt(const String &body, const char *key, long &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1) return false;
  size_t start = at, end = at;
  const char *s = body.c_str();
  if (s[start] == '-') ++end;
  while (end < body.length() && s[end] >= '0' && s[end] <= '9') ++end;
  if (end == start || (end == start + 1 && s[start] == '-')) return false;
  out = body.substring(start, end).toInt();
  return true;
}

bool jsonGetString(const String &body, const char *key, String &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1 || at >= body.length() || body[at] != '"') return false;
  ++at;
  String value;
  while (at < body.length() && body[at] != '"') {
    if (body[at] == '\\' && at + 1 < body.length()) ++at;  // Skip escapes.
    value += body[at];
    ++at;
  }
  out = value;
  return true;
}

// --- Response builders -------------------------------------------------------

String lightJson(const Bulb *b) {
  String j;
  j.reserve(220);
  j += "{\"id\":\"";
  j += bulbIeeeHex(b);
  j += "\",\"name\":\"";
  j += b->name;
  j += "\",\"online\":";
  j += b->online ? "true" : "false";
  j += ",\"on\":";
  j += b->state.power ? "true" : "false";
  j += ",\"brightness\":";
  j += bulbBrightnessPct(b);
  j += ",\"mode\":\"";
  j += b->state.mode == BulbColorMode::White ? "white" : "rgb";
  j += "\",\"kelvin\":";
  j += b->state.kelvin;
  j += ",\"rgb_hex\":\"#";
  char hex[7];
  snprintf(hex, sizeof(hex), "%02x%02x%02x", b->state.red, b->state.green, b->state.blue);
  j += hex;
  j += "\",\"endpoint\":";
  j += b->endpoint;
  j += ",\"short_addr\":\"0x";
  j += String(b->shortAddr, HEX);
  j += "\"}";
  return j;
}

void handleLightsGet() {
  String j = "[";
  for (size_t i = 0; i < registryCount(); ++i) {
    if (i > 0) j += ",";
    j += lightJson(registryGet(i));
  }
  j += "]";
  sendJson(200, j);
}

Bulb *bulbByApiId(const String &id) {
  for (size_t i = 0; i < registryCount(); ++i) {
    if (bulbIeeeHex(registryGet(i)).equalsIgnoreCase(id)) return registryGet(i);
  }
  return nullptr;
}

void handleLightPatch(const String &id) {
  Serial.printf("PATCH %s <- %s\n", id.c_str(), server.arg("plain").c_str());
  Bulb *b = bulbByApiId(id);
  if (b == nullptr) {
    sendJsonError(404, "unknown light id");
    return;
  }
  const String body = server.arg("plain");
  if (body.length() > 512) {
    sendJsonError(400, "body too large");
    return;
  }

  // Rename works offline; anything that drives the bulb does not.
  static const char *controlKeys[] = {"on", "brightness", "kelvin", "rgb_hex", "mode"};
  for (const char *key : controlKeys) {
    if (findJsonKey(body, key) != (size_t)-1 && !bulbReady(b)) {
      sendJsonError(409, "light is offline");
      return;
    }
  }

  bool changed = false;
  String name;
  if (jsonGetString(body, "name", name)) {
    if (name.length() > 0 && name.length() <= 20) {
      registryRename(b, name.c_str());
      changed = true;
    } else {
      sendJsonError(400, "invalid name");
      return;
    }
  }

  bool on;
  if (jsonGetBool(body, "on", on)) {
    on ? bulbSendOn(b) : bulbSendOff(b);
    changed = true;
  }

  long brightness;
  if (jsonGetInt(body, "brightness", brightness)) {
    bulbSendBrightness(b, (uint8_t)constrain(brightness, 0, 100), DEFAULT_TRANSITION_DS);
    changed = true;
  }

  long kelvin;
  if (jsonGetInt(body, "kelvin", kelvin)) {
    bulbSendKelvin(b, (int)constrain(kelvin, MIN_KELVIN, MAX_KELVIN), DEFAULT_TRANSITION_DS);
    changed = true;
  }

  String rgbHex;
  if (jsonGetString(body, "rgb_hex", rgbHex)) {
    unsigned r = 0, g = 0, bl = 0;
    if (rgbHex[0] == '#') rgbHex = rgbHex.substring(1);
    if (rgbHex.length() == 6 && sscanf(rgbHex.c_str(), "%02x%02x%02x", &r, &g, &bl) == 3) {
      bulbSendRgb(b, (uint8_t)r, (uint8_t)g, (uint8_t)bl, DEFAULT_TRANSITION_DS);
      changed = true;
    } else {
      sendJsonError(400, "invalid rgb_hex");
      return;
    }
  }

  String mode;
  if (jsonGetString(body, "mode", mode)) {
    if (mode == "white") {
      bulbSendKelvin(b, b->state.kelvin, DEFAULT_TRANSITION_DS);
      changed = true;
    } else if (mode == "rgb") {
      bulbSendRgb(b, b->state.red, b->state.green, b->state.blue, DEFAULT_TRANSITION_DS);
      changed = true;
    } else {
      sendJsonError(400, "invalid mode");
      return;
    }
  }

  if (!changed) {
    sendJsonError(400, "no recognized fields");
    return;
  }
  sendJson(200, lightJson(b));
}

void handleLightDelete(const String &id) {
  Bulb *b = bulbByApiId(id);
  if (b == nullptr) {
    sendJsonError(404, "unknown light id");
    return;
  }
  zigbeeRemoveDevice(b);
  sendJson(200, "{\"ok\":true}");
}

void handlePairingPost() {
  const String body = server.arg("plain");
  long seconds = PAIRING_SECONDS;
  jsonGetInt(body, "seconds", seconds);
  if (seconds < 30) seconds = 30;
  if (seconds > 600) seconds = 600;
  zigbeeOpenPairing((uint8_t)seconds);
  sendJson(200, "{\"open\":true,\"seconds\":" + String(seconds) + "}");
}

void handlePairingGet() {
  sendJson(200, String("{\"open\":") + (zigbeePairingActive() ? "true" : "false") +
                    ",\"seconds\":" + PAIRING_SECONDS + "}");
}

void handleStatusGet() {
  String j;
  j.reserve(300);
  j += "{\"version\":\"";
  j += FW_VERSION;
  j += "\",\"uptime_s\":";
  j += millis() / 1000;
  j += ",\"wifi\":";
  j += WiFi.status() == WL_CONNECTED ? "true" : "false";
  j += ",\"ip\":\"";
  j += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("");
  j += "\",\"rssi\":";
  j += WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  j += ",\"free_heap\":";
  j += ESP.getFreeHeap();
  j += ",\"bulbs\":";
  j += registryCount();
  j += ",\"hostname\":\"";
  j += hostnameValue;
  j += "\",\"pairing_open\":";
  j += zigbeePairingActive() ? "true" : "false";
  j += "}";
  sendJson(200, j);
}

// --- Scenes ------------------------------------------------------------------

void handleLightsAllPatch() {
  const String body = server.arg("plain");
  bool on;
  if (!jsonGetBool(body, "on", on)) {
    sendJsonError(400, "missing on");
    return;
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (!bulbReady(b)) continue;
    if (on) bulbSendOn(b);
    else bulbSendOff(b);
    ++applied;
  }
  sendJson(200, "{\"applied\":" + String(applied) + "}");
}

void handleScenesGet() {
  String j = "[";
  for (size_t i = 0; i < scenesCount(); ++i) {
    if (i > 0) j += ",";
    j += "{\"name\":\"" + sceneNameAt(i) + "\"}";
  }
  j += "]";
  sendJson(200, j);
}

void handleSceneCreate() {
  const String body = server.arg("plain");
  String name;
  if (!jsonGetString(body, "name", name)) {
    sendJsonError(400, "missing name");
    return;
  }
  name.toLowerCase();
  if (!isValidSceneName(name)) {
    sendJsonError(400, "invalid name (a-z, 0-9, '-', '_', max 20 chars)");
    return;
  }
  if (!sceneCapture(name)) {
    sendJsonError(500, "capture failed (no bulbs or storage full)");
    return;
  }
  sendJson(201, "{\"name\":\"" + name + "\"}");
}

void handleSceneCapture(const String &nameIn) {
  String name = nameIn;
  name.toLowerCase();
  if (!isValidSceneName(name)) {
    sendJsonError(400, "invalid name");
    return;
  }
  if (sceneIndexOf(name) < 0) {
    sendJsonError(404, "unknown scene");
    return;
  }
  if (!sceneCapture(name)) {
    sendJsonError(500, "capture failed");
    return;
  }
  sendJson(200, "{\"name\":\"" + name + "\"}");
}

void handleSceneRecall(const String &nameIn) {
  String name = nameIn;
  name.toLowerCase();
  int applied = 0, skipped = 0;
  if (!sceneRecall(name, applied, skipped)) {
    sendJsonError(404, "unknown scene");
    return;
  }
  sendJson(200, "{\"applied\":" + String(applied) + ",\"skipped\":" + String(skipped) + "}");
}

void handleSceneDelete(const String &nameIn) {
  String name = nameIn;
  name.toLowerCase();
  if (sceneIndexOf(name) < 0) {
    sendJsonError(404, "unknown scene");
    return;
  }
  sceneDelete(name);
  sendJson(200, "{\"ok\":true}");
}

bool isValidHostname(const String &name) {  if (name.length() == 0 || name.length() > MAX_HOSTNAME_LENGTH) return false;
  if (name[0] == '-' || name[name.length() - 1] == '-') return false;
  for (unsigned i = 0; i < name.length(); ++i) {
    char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

void handleHostnamePost() {
  const String body = server.arg("plain");
  String name;
  if (!jsonGetString(body, "hostname", name) || !webSetHostname(name)) {
    sendJsonError(400, "invalid hostname (a-z, 0-9, '-', 1-31 chars)");
    return;
  }
  sendJson(200, "{\"hostname\":\"" + hostnameValue + "\"}");
}

void dispatch() {
  const String uri = server.uri();
  const HTTPMethod method = server.method();

  if (uri == "/" && method == HTTP_GET) {
    server.send_P(200, "text/html", INDEX_HTML);
    return;
  }
  if (uri == "/api/lights" && method == HTTP_GET) {
    handleLightsGet();
    return;
  }
  if (uri == "/api/lights" && method == HTTP_PATCH) {
    handleLightsAllPatch();  // Collection update: command every bulb.
    return;
  }
  if (uri.startsWith("/api/lights/")) {
    const String id = uri.substring(strlen("/api/lights/"));
    if (method == HTTP_PATCH) {
      handleLightPatch(id);
      return;
    }
    if (method == HTTP_DELETE) {
      handleLightDelete(id);
      return;
    }
  }
  if (uri == "/api/scenes" && method == HTTP_GET) {
    handleScenesGet();
    return;
  }
  if (uri == "/api/scenes" && method == HTTP_POST) {
    handleSceneCreate();
    return;
  }
  if (uri.startsWith("/api/scenes/")) {
    // "/api/scenes/<name>" or "/api/scenes/<name>/recall"
    const String rest = uri.substring(strlen("/api/scenes/"));
    if (rest.endsWith("/recall") && method == HTTP_POST) {
      handleSceneRecall(rest.substring(0, rest.length() - 7));
      return;
    }
    if (method == HTTP_DELETE) {
      handleSceneDelete(rest);
      return;
    }
    if (method == HTTP_PATCH) {
      handleSceneCapture(rest);  // Re-capture under the same name.
      return;
    }
  }
  if (uri == "/api/pairing") {
    if (method == HTTP_POST) {
      handlePairingPost();
      return;
    }
    if (method == HTTP_GET) {
      handlePairingGet();
      return;
    }
  }
  if (uri == "/api/status" && method == HTTP_GET) {
    handleStatusGet();
    return;
  }
  if (uri == "/api/hostname" && method == HTTP_POST) {
    handleHostnamePost();
    return;
  }
  sendJsonError(404, "not found");
}

}  // namespace

// Public: shared by the REST API and the serial console.
bool webSetHostname(const String &name) {
  if (!isValidHostname(name)) return false;
  hostnameValue = name;
  webPrefs.begin(PREFS_NAMESPACE, false);
  webPrefs.putString("host", hostnameValue);
  webPrefs.end();
  if (mdnsUp) {  // Re-register under the new name.
    MDNS.end();
    mdnsUp = false;
  }
  Serial.printf("Hostname set to %s.local\n", hostnameValue.c_str());
  return true;
}

void webBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Wi-Fi: connecting to %s", WIFI_SSID);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi not connected yet; will keep retrying.");
  }

  webPrefs.begin(PREFS_NAMESPACE, false);
  hostnameValue = webPrefs.getString("host", DEFAULT_HOSTNAME);
  webPrefs.end();

  server.onNotFound(dispatch);
  server.begin();
}

void webTick() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetryMs >= WIFI_RETRY_MS) {
      lastWifiRetryMs = millis();
      Serial.println("Wi-Fi: retrying...");
      WiFi.reconnect();
    }
    if (mdnsUp) {
      MDNS.end();
      mdnsUp = false;
    }
    return;
  }

  if (!mdnsUp && MDNS.begin(hostnameValue.c_str())) {
    MDNS.addService("http", "tcp", WEB_PORT);
    mdnsUp = true;
    Serial.printf("mDNS: http://%s.local/\n", hostnameValue.c_str());
  }
}
