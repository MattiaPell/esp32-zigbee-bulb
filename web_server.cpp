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
#include "config_backup.h"
#include "json_lite.h"
#include "light_timers.h"
#include "mqtt_bridge.h"
#include "ota_update.h"
#include "scenes.h"
#include "web_hooks.h"
#include "web_icons.h"
#include "web_page.h"
#include "zigbee_bulbs.h"

namespace {

WebServer server(WEB_PORT);
bool mdnsUp = false;
uint32_t lastWifiRetryMs = 0;

// Headers kept for the OTA endpoint (optional MD5 checksum and, if set in
// secrets.h, the upload token).
const char *OTA_HEADERS[] = {"X-OTA-MD5", "X-OTA-TOKEN"};

Preferences webPrefs;

String hostnameValue = DEFAULT_HOSTNAME;

// --- JSON helpers live in json_lite.h (shared with MQTT and backup) ---------

void sendJson(int code, const String &payload) {
  server.send(code, "application/json", payload);
}

void sendJsonError(int code, const char *message) {
  server.send(code, "application/json", String("{\"error\":\"") + message + "\"}");
}

// --- Response builders -------------------------------------------------------

String lightDebugJson(const Bulb *b) {
  String j;
  j.reserve(160);
  j += "{\"lqi\":";
  j += b->lqi;
  j += ",\"rssi\":";
  j += b->rssi;
  j += ",\"last_seen_s\":";
  j += b->lastSeenMs == 0 ? -1 : (long)((millis() - b->lastSeenMs) / 1000);
  j += ",\"cmd_sent\":";
  j += b->cmdSent;
  j += ",\"cmd_failed\":";
  j += b->cmdFailed;
  j += ",\"last_fail\":\"";
  j += b->lastFailStatus == 0xFF
           ? "none"
           : esp_zb_zcl_status_to_name((esp_zb_zcl_status_t)b->lastFailStatus);
  j += "\"}";
  return j;
}

String lightJson(const Bulb *b, bool withDebug = false) {
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
  j += "\",\"timer\":";
  j += bulbTimerRemaining(b);
  j += ",\"endpoint\":";
  j += b->endpoint;
  j += ",\"short_addr\":\"0x";
  j += String(b->shortAddr, HEX);
  j += "\"";
  if (withDebug) {
    j += ",\"debug\":";
    j += lightDebugJson(b);
  }
  j += "}";
  return j;
}

void handleLightsGet() {
  const bool debug = server.hasArg("debug");
  String j = "[";
  for (size_t i = 0; i < registryCount(); ++i) {
    if (i > 0) j += ",";
    j += lightJson(registryGet(i), debug);
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

void handleLightDebugGet(const String &id) {
  Bulb *b = bulbByApiId(id);
  if (b == nullptr) {
    sendJsonError(404, "unknown light id");
    return;
  }
  String j;
  j.reserve(280);
  j += "{\"id\":\"";
  j += bulbIeeeHex(b);
  j += "\",\"name\":\"";
  j += b->name;
  j += "\",\"online\":";
  j += b->online ? "true" : "false";
  j += ",\"ready\":";
  j += bulbReady(b) ? "true" : "false";
  j += ",\"endpoint\":";
  j += b->endpoint;
  j += ",\"short_addr\":\"0x";
  j += String(b->shortAddr, HEX);
  j += "\",\"debug\":";
  j += lightDebugJson(b);
  j += "}";
  sendJson(200, j);
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

// --- Devices (read-only network diagnostics) ----------------------------------

void handleDevicesGet() {
  DeviceInfo devs[24];
  const size_t a = zigbeeMemberSnapshot(devs, 12);
  const size_t b = zigbeeBoundSnapshot(devs + a, 24 - a);
  const size_t n = a + b;
  String j = "[";
  for (size_t i = 0; i < n; ++i) {
    // Deduplicate: a bound device may also appear in the neighbor table.
    bool dup = false;
    for (size_t p = 0; p < i; ++p) {
      if (memcmp(devs[p].ieee, devs[i].ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
        dup = true;
        break;
      }
    }
    if (dup) continue;
    if (j.length() > 1) j += ",";
    j += "{\"ieee\":\"";
    Bulb fake;  // Reuse the shared hex formatter.
    memcpy(fake.ieee, devs[i].ieee, sizeof(esp_zb_ieee_addr_t));
    j += bulbIeeeHex(&fake);
    j += "\",\"short\":\"0x";
    j += String(devs[i].shortAddr, HEX);
    j += "\",\"type\":\"";
    if (devs[i].deviceType == 0) j += "coordinator";
    else if (devs[i].deviceType == 1) j += "router";
    else if (devs[i].deviceType == 2) j += "end-device";
    else j += "bound";
    j += "\",\"name\":\"";
    Bulb *b2 = registryFindByIeee(devs[i].ieee);
    j += b2 != nullptr ? b2->name : "";
    j += "\"}";
  }
  j += "]";
  sendJson(200, j);
}

void handleStatusGet() {  String j;
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
  j += ",\"timer_all\":";
  j += bulbTimerRemainingAll();
  j += ",\"ota\":{\"slot\":\"";
  j += otaRunningSlot();
  j += "\",\"pending_verify\":";
  j += otaPendingVerify() ? "true" : "false";
  j += "}";
  sendJson(200, j);
}

// --- OTA firmware upload ------------------------------------------------------

// Multipart upload streaming: the file part is piped into the ota_update
// state machine. WebServer is single-threaded, so while the upload is being
// parsed no other request reaches dispatch().
void handleOtaUpload() {
  HTTPUpload &upload = server.upload();
  switch (upload.status) {
    case UPLOAD_FILE_START: {
      bool allowed = true;
#ifdef OTA_TOKEN
      allowed = server.header("X-OTA-TOKEN").equals(OTA_TOKEN);
#endif
      if (allowed) {
        otaUploadStart(server.header("X-OTA-MD5"));
      } else {
        otaUploadReject();
      }
      break;
    }
    case UPLOAD_FILE_WRITE:
      otaUploadWrite(upload.buf, upload.currentSize);
      break;
    case UPLOAD_FILE_END:
      otaUploadEnd();
      break;
    case UPLOAD_FILE_ABORTED:
      otaUploadAbort();
      break;
  }
}

void handleOtaPost() {
  switch (otaResult()) {
    case OtaResult::Done:
      // otaTick() reboots shortly after this response reaches the client.
      sendJson(200, String("{\"ok\":true,\"size\":") + otaBytesWritten() + "}");
      return;
    case OtaResult::Rejected:
      sendJsonError(403, "ota token missing or wrong");
      return;
    case OtaResult::BeginFailed:
      sendJsonError(500, "ota begin failed (no free slot)");
      return;
    case OtaResult::WriteFailed:
      sendJsonError(500, "ota write failed (image too large?)");
      return;
    case OtaResult::FinishFailed:
      sendJsonError(400, "ota finish failed (corrupt image or md5 mismatch)");
      return;
    case OtaResult::Aborted:
      sendJsonError(400, "upload aborted");
      return;
    default:
      sendJsonError(400, "no firmware data");
  }
}

// --- Scenes ------------------------------------------------------------------

void handleLightsAllPatch() {
  const String body = server.arg("plain");
  bool on = false;
  long brightness = 0, kelvin = 0;
  String rgbHex;
  const bool hasOn = jsonGetBool(body, "on", on);
  const bool hasBrightness = jsonGetInt(body, "brightness", brightness);
  const bool hasKelvin = jsonGetInt(body, "kelvin", kelvin);
  const bool hasRgb = jsonGetString(body, "rgb_hex", rgbHex);
  if (!hasOn && !hasBrightness && !hasKelvin && !hasRgb) {
    sendJsonError(400, "no recognized fields (on, brightness, kelvin, rgb_hex)");
    return;
  }

  if (hasOn && !on) {
    sendJson(200, "{\"applied\":" + String(bulbSendAllOff()) + "}");
    return;
  }

  int applied = 0;
  if (hasOn && on) applied = bulbSendAllOn();
  if (hasBrightness) {
    applied = bulbSendAllBrightness((uint8_t)constrain(brightness, 0, 100),
                                    DEFAULT_TRANSITION_DS);
  }
  if (hasKelvin) {
    applied = bulbSendAllKelvin((int)constrain(kelvin, MIN_KELVIN, MAX_KELVIN),
                                DEFAULT_TRANSITION_DS);
  }
  if (hasRgb) {
    if (rgbHex[0] == '#') rgbHex = rgbHex.substring(1);
    unsigned r = 0, g = 0, bl = 0;
    if (rgbHex.length() != 6 ||
        sscanf(rgbHex.c_str(), "%02x%02x%02x", &r, &g, &bl) != 3) {
      sendJsonError(400, "invalid rgb_hex");
      return;
    }
    applied = bulbSendAllRgb((uint8_t)r, (uint8_t)g, (uint8_t)bl,
                             DEFAULT_TRANSITION_DS);
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

// --- Webhooks -----------------------------------------------------------------

void handleHooksGet() {
  String j = "{\"urls\":[";
  for (size_t i = 0; i < webHookUrlCount(); ++i) {
    if (i > 0) j += ",";
    j += "\"";
    j += webHookUrlAt(i);
    j += "\"";
  }
  j += "]}";
  sendJson(200, j);
}

void handleHooksPost() {
  const String body = server.arg("plain");
  String url;
  if (!jsonGetString(body, "url", url) || !webHookUrlAdd(url)) {
    sendJsonError(400, "invalid url (http:// or https://, max 120 chars)");
    return;
  }
  sendJson(201, "{\"ok\":true}");
}

void handleHooksDelete() {
  const String body = server.arg("plain");
  String url;
  if (!jsonGetString(body, "url", url) || !webHookUrlRemove(url)) {
    sendJsonError(404, "unknown url");
    return;
  }
  sendJson(200, "{\"ok\":true}");
}

void handleHookTestPost() {
  const String body = server.arg("plain");
  String url;
  if (!jsonGetString(body, "url", url) || !webHookTest(url)) {
    sendJsonError(400, "invalid url (http:// or https://, max 120 chars)");
    return;
  }
  sendJson(200, "{\"queued\":true}");
}

// --- MQTT bridge --------------------------------------------------------------

String mqttStatusJson() {
  String j = "{\"enabled\":";
  j += mqttEnabledRuntime() ? "true" : "false";
  j += ",\"connected\":";
  j += mqttConnected() ? "true" : "false";
  j += ",\"host\":\"";
#ifdef MQTT_HOST
  j += MQTT_HOST;
  j += "\",\"port\":";
  j += MQTT_PORT;
#else
  j += "\",\"port\":0";
#endif
  j += "}";
  return j;
}

#ifdef MQTT_HOST

void handleMqttGet() {
  sendJson(200, mqttStatusJson());
}

void handleMqttPost() {
  const String body = server.arg("plain");
  bool enabled;
  if (!jsonGetBool(body, "enabled", enabled)) {
    sendJsonError(400, "missing enabled");
    return;
  }
  mqttSetEnabled(enabled);
  sendJson(200, mqttStatusJson());
}

#endif  // MQTT_HOST

// --- Backup / restore ------------------------------------------------------------

void handleBackupGet() {
  sendJson(200, configBackupJson());
}

void handleRestorePost() {
  const String body = server.arg("plain");
  if (body.length() > 12288) {
    sendJsonError(400, "body too large");
    return;
  }
  String summary;
  if (!configRestoreJson(body, summary)) {
    sendJsonError(400, "not a valid backup document");
    return;
  }
  sendJson(200, "{\"ok\":true,\"restored\":" + summary + "}");
}

// --- Timers ------------------------------------------------------------------

// POST /api/lights/{id}/timer and /api/timer: {"seconds":N} (0 cancels).
void handleLightTimerPost(const String &id) {
  Bulb *b = bulbByApiId(id);
  if (b == nullptr) {
    sendJsonError(404, "unknown light id");
    return;
  }
  const String body = server.arg("plain");
  long seconds;
  if (!jsonGetInt(body, "seconds", seconds)) {
    sendJsonError(400, "missing seconds");
    return;
  }
  if (seconds < 0 || seconds > (long)(24UL * 60 * 60)) {
    sendJsonError(400, "seconds out of range (0-86400)");
    return;
  }
  bulbTimerSet(b, (uint32_t)seconds);
  sendJson(200, "{\"timer\":" + String(bulbTimerRemaining(b)) + "}");
}

void handleAllTimerPost() {
  const String body = server.arg("plain");
  long seconds;
  if (!jsonGetInt(body, "seconds", seconds)) {
    sendJsonError(400, "missing seconds");
    return;
  }
  if (seconds < 0 || seconds > (long)(24UL * 60 * 60)) {
    sendJsonError(400, "seconds out of range (0-86400)");
    return;
  }
  bulbTimerSetAll((uint32_t)seconds);
  sendJson(200, "{\"timer\":" + String(bulbTimerRemainingAll()) + "}");
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
  if (method == HTTP_GET) {  // Static PWA assets.
    if (uri == "/manifest.json") {
      server.send_P(200, "application/manifest+json", MANIFEST_JSON);
      return;
    }
    if (uri == "/sw.js") {
      server.send_P(200, "application/javascript", SW_JS);
      return;
    }
    if (uri == "/icon-192.png") {
      server.send_P(200, "image/png", (PGM_P)ICON_192, ICON_192_LEN);
      return;
    }
    if (uri == "/icon-512.png") {
      server.send_P(200, "image/png", (PGM_P)ICON_512, ICON_512_LEN);
      return;
    }
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
    const String rest = uri.substring(strlen("/api/lights/"));
    if (rest.endsWith("/timer") && method == HTTP_POST) {
      handleLightTimerPost(rest.substring(0, rest.length() - 6));
      return;
    }
    if (rest.endsWith("/debug") && method == HTTP_GET) {
      handleLightDebugGet(rest.substring(0, rest.length() - 6));
      return;
    }
    if (method == HTTP_PATCH) {
      handleLightPatch(rest);
      return;
    }
    if (method == HTTP_DELETE) {
      handleLightDelete(rest);
      return;
    }
  }
  if (uri == "/api/timer" && method == HTTP_POST) {
    handleAllTimerPost();
    return;
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
  if (uri == "/api/devices" && method == HTTP_GET) {
    handleDevicesGet();
    return;
  }
  if (uri == "/api/hostname" && method == HTTP_POST) {
    handleHostnamePost();
    return;
  }
  if (uri == "/api/hooks" && method == HTTP_GET) {
    handleHooksGet();
    return;
  }
  if (uri == "/api/hooks" && method == HTTP_POST) {
    handleHooksPost();
    return;
  }
  if (uri == "/api/hooks" && method == HTTP_DELETE) {
    handleHooksDelete();
    return;
  }
  if (uri == "/api/hooks/test" && method == HTTP_POST) {
    handleHookTestPost();
    return;
  }
#ifdef MQTT_HOST
  if (uri == "/api/mqtt") {
    if (method == HTTP_GET) {
      handleMqttGet();
      return;
    }
    if (method == HTTP_POST) {
      handleMqttPost();
      return;
    }
  }
#endif
  if (uri == "/api/backup" && method == HTTP_GET) {
    handleBackupGet();
    return;
  }
  if (uri == "/api/restore" && method == HTTP_POST) {
    handleRestorePost();
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

  server.on("/api/ota", HTTP_POST, handleOtaPost, handleOtaUpload);
  server.collectHeaders(OTA_HEADERS, sizeof(OTA_HEADERS) / sizeof(OTA_HEADERS[0]));
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
