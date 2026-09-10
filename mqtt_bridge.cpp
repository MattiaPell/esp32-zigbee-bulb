#include "mqtt_bridge.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy secrets.example.h to secrets.h and enter your Wi-Fi credentials"
#endif

#include "debug_log.h"

#ifdef MQTT_HOST

#include <Preferences.h>
#include <WiFiClient.h>
#include <WiFi.h>

#include "bulb_registry.h"
#include "config.h"
#include "json_lite.h"
#include "zigbee_bulbs.h"

// Command router (defined at the bottom, called from the packet parser).
void handleMqttCommand(const String &topic, const String &payload);

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_PREFIX
#define MQTT_PREFIX "bulbctl"
#endif
#ifndef MQTT_DISCOVERY
#define MQTT_DISCOVERY 1
#endif

namespace {

// MQTT control packet types (3.1.1).
constexpr uint8_t PKT_CONNACK = 2;
constexpr uint8_t PKT_PUBLISH = 3;
constexpr uint8_t PKT_SUBACK = 9;
constexpr uint8_t PKT_PINGRESP = 13;

void onConnected();
void dispatchPacket(uint8_t fixedHeader, const uint8_t *data, size_t len);

enum class MqttState : uint8_t { Disconnected, Connecting, Connected };

WiFiClient client;
MqttState state = MqttState::Disconnected;
bool runtimeEnabled = true;

uint32_t nextConnectAtMs = 0;
uint8_t connectAttempts = 0;
uint32_t lastPingMs = 0;
uint32_t lastPublishMs = 0;
uint32_t lastDiagMs = 0;

String inBuf;

// Publish cache: (serialized state + online) per bulb, so unchanged bulbs
// are not republished every poll.
String publishSig[MAX_BULBS];
bool sigKnown[MAX_BULBS] = {false};

String willTopic;

Preferences prefs;

// --- Encoding helpers --------------------------------------------------------

size_t encodeVarint(uint32_t value, uint8_t *out) {
  size_t n = 0;
  do {
    uint8_t byte = value % 128;
    value /= 128;
    if (value > 0) byte |= 0x80;
    out[n++] = byte;
  } while (value > 0);
  return n;
}

void writeMqttString(uint8_t *out, size_t &pos, const char *text) {
  const size_t len = strlen(text);
  out[pos++] = (uint8_t)(len >> 8);
  out[pos++] = (uint8_t)(len & 0xFF);
  memcpy(out + pos, text, len);
  pos += len;
}

bool sendBytes(const uint8_t *data, size_t len) {
  return client.write(data, len) == len;
}

String topicFor(const char *suffix) {
  return String(MQTT_PREFIX) + suffix;
}

String topicForBulb(const Bulb *b, const char *suffix) {
  return String(MQTT_PREFIX) + "/" + bulbIeeeHex(b) + suffix;
}

String miredsOf(const BulbState &s) {
  return String((uint16_t)(1000000UL / (uint32_t)s.kelvin));
}

String stateJson(const Bulb *b) {
  const BulbState &s = b->state;
  String j;
  j.reserve(150);
  j += "{\"state\":\"";
  j += s.power ? "ON" : "OFF";
  j += "\",\"brightness\":";
  j += s.level;
  j += ",\"color_mode\":\"";
  j += s.mode == BulbColorMode::White ? "color_temp" : "rgb";
  j += "\"";
  if (s.mode == BulbColorMode::White) {
    j += ",\"color_temp\":";
    j += miredsOf(s);
  } else {
    j += ",\"color\":{\"r\":";
    j += s.red;
    j += ",\"g\":";
    j += s.green;
    j += ",\"b\":";
    j += s.blue;
    j += "}";
  }
  j += "}";
  return j;
}

String bridgeUid() {
  return String(MQTT_PREFIX) + "-" + storedHostname();
}

String bulbUid(const Bulb *b) {
  return bridgeUid() + "-" + bulbIeeeHex(b);
}

void appendBridgeDevice(String &j) {
  j += "\"device\":{\"identifiers\":[\"";
  j += bridgeUid();
  j += "\"],\"name\":\"";
  j += storedHostname();
  j += "\",\"manufacturer\":\"MattiaPell\",\"model\":\"esp32-zigbee-bulb\",\"sw_version\":\"";
  j += FW_VERSION;
  j += "\"}";
}

void appendBulbDevice(String &j, const Bulb *b) {
  j += "\"device\":{\"identifiers\":[\"";
  j += bulbUid(b);
  j += "\"],\"name\":\"";
  j += b->name;
  j += "\",\"manufacturer\":\"IKEA\",\"via_device\":\"";
  j += bridgeUid();
  j += "\"}";
}

// Availability shared by every entity: bulb and bridge must both be online.
void appendAvailability(String &j, const Bulb *b) {
  j += "\"availability\":[{\"topic\":\"";
  j += topicForBulb(b, "/availability");
  j += "\"},{\"topic\":\"";
  j += willTopic;
  j += "\"}],\"availability_mode\":\"all\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\"";
}

String discoveryJson(const Bulb *b) {
  const bool rgb = b->state.mode == BulbColorMode::Rgb;

  String j;
  j.reserve(700);
  j += "{\"name\":\"";
  j += b->name;
  j += "\",\"unique_id\":\"";
  j += bulbUid(b);
  j += "\",\"state_topic\":\"";
  j += topicForBulb(b, "/state");
  j += "\",\"command_topic\":\"";
  j += topicForBulb(b, "/set");
  j += "\",\"brightness\":true,\"brightness_scale\":255,\"color_mode\":true,\"supported_color_modes\":[";
  j += rgb ? "\"color_temp\",\"rgb\"" : "\"color_temp\"";
  j += "],\"min_mireds\":250,\"max_mireds\":455,";
  appendAvailability(j, b);
  j += ",";
  appendBulbDevice(j, b);
  j += "}";
  return j;
}

// topic + payload publish (QoS 0, optional retain).
void publish(const char *topic, const String &payload, bool retain) {
  if (state != MqttState::Connected) return;
  const size_t topicLen = strlen(topic);
  const uint32_t remaining = 2 + topicLen + payload.length();
  uint8_t head[5];
  head[0] = 0x30 | (retain ? 0x01 : 0x00);
  const size_t varintLen = encodeVarint(remaining, head + 1);
  if (!sendBytes(head, 1 + varintLen)) return;
  uint8_t tlen[2] = {(uint8_t)(topicLen >> 8), (uint8_t)(topicLen & 0xFF)};
  sendBytes(tlen, 2);
  sendBytes((const uint8_t *)topic, topicLen);
  sendBytes((const uint8_t *)payload.c_str(), payload.length());
}

void publishBulb(const Bulb *b) {
  const String st = topicForBulb(b, "/state");
  publish(st.c_str(), stateJson(b), true);
  const String av = topicForBulb(b, "/availability");
  publish(av.c_str(), b->online ? "online" : "offline", true);
}

void publishDiscovery(const Bulb *b) {
#if MQTT_DISCOVERY
  const String cfgTopic =
      "homeassistant/light/" + String(MQTT_PREFIX) + "-" + storedHostname() +
      "-" + bulbIeeeHex(b) + "/light/config";
  publish(cfgTopic.c_str(), discoveryJson(b), true);
#endif
}

// The controller itself, as a connectivity binary_sensor. Its device id is
// also the `via_device` of every bulb, so HA nests the lights under the bridge.
void publishBridgeDiscovery() {
#if MQTT_DISCOVERY
  const String topic = "homeassistant/binary_sensor/" + bridgeUid() + "/config";
  String j;
  j.reserve(360);
  j += "{\"name\":\"Bridge MQTT\",\"unique_id\":\"";
  j += bridgeUid();
  j += "\",\"state_topic\":\"";
  j += willTopic;
  j += "\",\"device_class\":\"connectivity\",\"payload_on\":\"online\",\"payload_off\":\"offline\",";
  appendBridgeDevice(j);
  j += "}";
  publish(topic.c_str(), j, true);
#endif
}

// Per-bulb link diagnostics (LQI, RSSI, seconds since last seen).
void publishDiagnosticDiscovery(const Bulb *b) {
#if MQTT_DISCOVERY
  const String uid = bulbUid(b);

  String lqi;
  lqi.reserve(440);
  lqi += "{\"name\":\"LQI\",\"unique_id\":\"" + uid + "-lqi\",\"state_topic\":\"" +
         topicForBulb(b, "/lqi") +
         "\",\"icon\":\"mdi:signal\",\"entity_category\":\"diagnostic\",";
  appendAvailability(lqi, b);
  lqi += ",";
  appendBulbDevice(lqi, b);
  lqi += "}";
  publish((String("homeassistant/sensor/") + uid + "-lqi/config").c_str(), lqi, true);

  String rssi;
  rssi.reserve(480);
  rssi += "{\"name\":\"RSSI\",\"unique_id\":\"" + uid +
          "-rssi\",\"state_topic\":\"" + topicForBulb(b, "/rssi") +
          "\",\"unit_of_measurement\":\"dBm\",\"device_class\":\"signal_strength\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\",";
  appendAvailability(rssi, b);
  rssi += ",";
  appendBulbDevice(rssi, b);
  rssi += "}";
  publish((String("homeassistant/sensor/") + uid + "-rssi/config").c_str(), rssi, true);

  String seen;
  seen.reserve(510);
  seen += "{\"name\":\"Ultimo contatto\",\"unique_id\":\"" + uid +
          "-seen\",\"state_topic\":\"" + topicForBulb(b, "/last_seen") +
          "\",\"unit_of_measurement\":\"s\",\"device_class\":\"duration\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\",";
  appendAvailability(seen, b);
  seen += ",";
  appendBulbDevice(seen, b);
  seen += "}";
  publish((String("homeassistant/sensor/") + uid + "-seen/config").c_str(), seen, true);
#endif
}

void publishDiagnostics(const Bulb *b) {
  const String lqi = topicForBulb(b, "/lqi");
  publish(lqi.c_str(), String((unsigned)b->lqi), true);
  const String rssi = topicForBulb(b, "/rssi");
  publish(rssi.c_str(), String((int)b->rssi), true);
  const String seen = topicForBulb(b, "/last_seen");
  if (b->lastSeenMs == 0) {
    publish(seen.c_str(), "unknown", true);
  } else {
    publish(seen.c_str(),
            String((unsigned long)((millis() - b->lastSeenMs) / 1000)), true);
  }
}

// --- Incoming packet parsing (incremental, non-blocking) --------------------

void dispatchPacket(uint8_t fixedHeader, const uint8_t *data, size_t len);

bool parseIncoming() {
  while (client.available() > 0) {
    if (inBuf.length() >= 1024) {  // Runaway buffer: connection is garbage.
      inBuf = "";
      return false;
    }
    inBuf += (char)client.read();
    if (inBuf.length() < 2) continue;

    const uint8_t *d = (const uint8_t *)inBuf.c_str();
    const size_t bufLen = inBuf.length();
    uint32_t remLen = 0;
    size_t remBytes = 0;
    uint32_t mult = 1;
    bool varintDone = false;
    bool malformed = false;
    for (size_t i = 1; i < bufLen; ++i) {
      remLen += (uint32_t)(d[i] & 0x7F) * mult;
      mult *= 128;
      ++remBytes;
      if ((d[i] & 0x80) == 0) {
        varintDone = true;
        break;
      }
      if (remBytes == 4) {
        malformed = true;
        break;
      }
    }
    if (malformed) {
      inBuf = "";
      return false;
    }
    if (!varintDone) continue;
    const size_t headerLen = 1 + remBytes;
    if (bufLen < headerLen + remLen) continue;  // Wait for the full packet.

    dispatchPacket(d[0], d + headerLen, remLen);
    inBuf.remove(0, headerLen + remLen);
    return true;  // One packet per tick keeps the loop responsive.
  }
  return false;
}

void dispatchPacket(uint8_t fixedHeader, const uint8_t *data, size_t len) {
  const uint8_t type = fixedHeader >> 4;
  if (type == PKT_CONNACK) {
    if (len >= 2 && data[1] == 0) {
      onConnected();  // Session accepted.
    }
    return;
  }
  if (type == PKT_PUBLISH) {
    if (len < 2) return;
    const uint16_t topicLen = ((uint16_t)data[0] << 8) | data[1];
    if (2 + topicLen > len) return;
    const uint8_t qos = (fixedHeader >> 1) & 0x03;
    const size_t payloadAt = 2 + topicLen + (qos > 0 ? 2 : 0);
    if (payloadAt > len) return;
    const String topic((const char *)data + 2, topicLen);
    const String payload((const char *)data + payloadAt, len - payloadAt);
    handleMqttCommand(topic, payload);
  }
  // CONNACK / SUBACK / PINGRESP are handled by the connection flow or
  // simply counted as server liveness.
}

// --- Command handling --------------------------------------------------------

bool parseRgbPayload(const String &payload, uint8_t &r, uint8_t &g, uint8_t &bl) {
  const int at = payload.indexOf("\"color\"");
  if (at < 0) return false;
  const int brace = payload.indexOf('{', at);
  if (brace < 0) return false;
  const String obj = payload.substring(brace);
  long rr = 0, gg = 0, bb = 0;
  if (!jsonGetInt(obj, "r", rr) || !jsonGetInt(obj, "g", gg) ||
      !jsonGetInt(obj, "b", bb)) {
    return false;
  }
  r = (uint8_t)constrain(rr, 0, 255);
  g = (uint8_t)constrain(gg, 0, 255);
  bl = (uint8_t)constrain(bb, 0, 255);
  return true;
}

uint8_t levelToPct(uint16_t level) {
  return (uint8_t)(((uint32_t)level * 100 + 127) / 255);
}

void applyPerBulb(Bulb *b, const String &payload) {
  String stateStr;
  const bool hasState = jsonGetString(payload, "state", stateStr);
  long brightness = 0, mireds = 0;
  const bool hasBrightness = jsonGetInt(payload, "brightness", brightness);
  const bool hasTemp = jsonGetInt(payload, "color_temp", mireds);
  uint8_t r, g, bl;
  const bool hasRgb = parseRgbPayload(payload, r, g, bl);

  if (hasState && stateStr.equalsIgnoreCase("OFF")) {
    bulbSendOff(b);
    return;
  }
  if (hasBrightness && brightness > 0) {
    bulbSendBrightness(b, levelToPct((uint16_t)brightness), DEFAULT_TRANSITION_DS);
  }
  if (hasTemp && mireds > 0) {
    bulbSendKelvin(b, (int)constrain(1000000L / mireds, MIN_KELVIN, MAX_KELVIN),
                   DEFAULT_TRANSITION_DS);
  }
  if (hasRgb) {
    bulbSendRgb(b, r, g, bl, DEFAULT_TRANSITION_DS);
  }
  if (!hasBrightness && !hasTemp && !hasRgb) {
    if (hasState && stateStr.equalsIgnoreCase("TOGGLE")) {
      bulbSendToggle(b);
    } else {
      bulbSendOn(b);
    }
  }
}

void applyToAll(const String &payload) {
  String stateStr;
  const bool hasState = jsonGetString(payload, "state", stateStr);
  long brightness = 0, mireds = 0;
  const bool hasBrightness = jsonGetInt(payload, "brightness", brightness);
  const bool hasTemp = jsonGetInt(payload, "color_temp", mireds);
  uint8_t r, g, bl;
  const bool hasRgb = parseRgbPayload(payload, r, g, bl);

  if (hasState && stateStr.equalsIgnoreCase("OFF")) {
    bulbSendAllOff();
    return;
  }
  if (hasBrightness && brightness > 0) {
    bulbSendAllBrightness(levelToPct((uint16_t)brightness), DEFAULT_TRANSITION_DS);
  }
  if (hasTemp && mireds > 0) {
    bulbSendAllKelvin((int)constrain(1000000L / mireds, MIN_KELVIN, MAX_KELVIN),
                      DEFAULT_TRANSITION_DS);
  }
  if (hasRgb) bulbSendAllRgb(r, g, bl, DEFAULT_TRANSITION_DS);
  if (!hasBrightness && !hasTemp && !hasRgb && hasState) bulbSendAllOn();
}

}  // namespace

// Public command router (called by dispatchPacket above).
void handleMqttCommand(const String &topic, const String &payload) {
  const String prefix = String(MQTT_PREFIX) + "/";
  if (!topic.startsWith(prefix) || !topic.endsWith("/set")) return;
  const String target = topic.substring(prefix.length(), topic.length() - 4);
  if (target == "all") {
    applyToAll(payload);
    return;
  }
  Bulb *b = registryFindByIeeeHex(target);
  if (b != nullptr) applyPerBulb(b, payload);
}

// --- Connection lifecycle -----------------------------------------------------

namespace {

bool sendConnectPacket() {
  // Client id fresh at every connect: picks up hostname changes.
  String idStr = "bulbctl-" + storedHostname();
  if (idStr.length() > 36) idStr = idStr.substring(0, 36);
  uint8_t id[40];
  idStr.toCharArray((char *)id, sizeof(id));
  uint8_t user[33], pass[33];
  uint8_t will[20];
  willTopic.toCharArray((char *)will, sizeof(will));

  const char *userStr =
#if defined(MQTT_USER)
      MQTT_USER;
#else
      "";
#endif
  const char *passStr =
#if defined(MQTT_PASS)
      MQTT_PASS;
#else
      "";
#endif
  const bool hasUser = userStr[0] != '\0';
  const bool hasPass = passStr[0] != '\0';
  strlcpy((char *)user, userStr, sizeof(user));
  strlcpy((char *)pass, passStr, sizeof(pass));

  const size_t idLen = strlen((char *)id);
  const size_t willLen = strlen((char *)will);
  const size_t userLen = hasUser ? strlen((char *)user) : 0;
  const size_t passLen = hasPass ? strlen((char *)pass) : 0;

  uint32_t remaining = 10;  // Protocol name (6) + level (1) + flags (1) + keepalive (2)
  remaining += 2 + idLen + 2 + willLen + 2 + 7;  // Client id + will + "offline"
  if (hasUser) remaining += 2 + userLen;
  if (hasPass) remaining += 2 + passLen;

  uint8_t head[5];
  head[0] = 0x10;
  const size_t headLen = 1 + encodeVarint(remaining, head + 1);
  if (!sendBytes(head, headLen)) return false;

  uint8_t body[200];
  size_t pos = 0;
  const uint8_t proto[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04};
  memcpy(body + pos, proto, sizeof(proto));
  pos += sizeof(proto);
  body[pos++] = 0x24 | (hasUser ? 0x80 : 0x00) | (hasPass ? 0x40 : 0x00);
  body[pos++] = 0x00;
  body[pos++] = (uint8_t)MQTT_KEEPALIVE_S;
  writeMqttString(body, pos, (char *)id);
  writeMqttString(body, pos, (char *)will);
  writeMqttString(body, pos, "offline");
  if (hasUser) writeMqttString(body, pos, (char *)user);
  if (hasPass) writeMqttString(body, pos, (char *)pass);
  return sendBytes(body, pos);
}

bool sendSubscribePacket() {
  const String filter = topicFor("/+/set");
  const uint8_t pkt[] = {
      0x82, (uint8_t)(5 + filter.length()), 0x00, 0x01,
      (uint8_t)(filter.length() >> 8), (uint8_t)(filter.length() & 0xFF)};
  if (!sendBytes(pkt, sizeof(pkt))) return false;
  return sendBytes((const uint8_t *)filter.c_str(), filter.length()) &&
         sendBytes((const uint8_t *)"\x00", 1);
}

void onConnected() {
  state = MqttState::Connected;
  connectAttempts = 0;
  lastPingMs = millis();
  inBuf = "";
  for (size_t i = 0; i < MAX_BULBS; ++i) {
    sigKnown[i] = false;  // Force a full state republish.
  }
  publish(willTopic.c_str(), "online", true);
  sendSubscribePacket();
  publishBridgeDiscovery();
  for (size_t i = 0; i < registryCount(); ++i) {
    publishDiscovery(registryGet(i));
    publishDiagnosticDiscovery(registryGet(i));
    publishDiagnostics(registryGet(i));
  }
  lastDiagMs = millis();
  debugLogPrintf("MQTT: connected to %s:%u\n", MQTT_HOST, (unsigned)MQTT_PORT);
}

bool tryConnect() {
  client.stop();
  if (!client.connect(MQTT_HOST, MQTT_PORT)) return false;
  if (!sendConnectPacket()) {
    client.stop();
    return false;
  }
  return true;
}

void scheduleReconnect() {
  ++connectAttempts;
  uint32_t delayMs = MQTT_CONNECT_BACKOFF_MS * connectAttempts;
  if (delayMs > MQTT_RECONNECT_MAX_MS) delayMs = MQTT_RECONNECT_MAX_MS;
  nextConnectAtMs = millis() + delayMs;
  state = MqttState::Disconnected;
  if (connectAttempts == 1 || (connectAttempts % 10) == 0) {
    debugLogPrintf("MQTT: reconnecting in %lu ms (attempt %u)\n",
                  (unsigned long)delayMs, connectAttempts);
  }
}

void republishChanged() {
  const uint32_t now = millis();
  if (now - lastPublishMs < MQTT_REPUBLISH_MS) return;
  lastPublishMs = now;
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    const String sig =
        serializeBulbState(b->state) + (b->online ? "|on" : "|off");
    if (sigKnown[i] && sig == publishSig[i]) continue;
    publishBulb(b);
    publishSig[i] = sig;
    sigKnown[i] = true;
    return;  // At most one bulb per tick.
  }
}

}  // namespace

void mqttBegin() {
  prefs.begin("mqtt", false);
  runtimeEnabled = prefs.getBool("on", true);

  willTopic = topicFor("/bridge");

  if (!runtimeEnabled) state = MqttState::Disconnected;
  debugLogPrintf("MQTT: bridge %s (%s:%u)\n", runtimeEnabled ? "enabled" : "disabled",
                MQTT_HOST, (unsigned)MQTT_PORT);
}

void mqttTick() {
  if (!runtimeEnabled) return;
  if (WiFi.status() != WL_CONNECTED) return;

  const uint32_t now = millis();

  switch (state) {
    case MqttState::Disconnected:
      if ((int32_t)(now - nextConnectAtMs) >= 0) {
        if (tryConnect()) {
          state = MqttState::Connecting;
        } else {
          scheduleReconnect();
        }
      }
      break;

    case MqttState::Connecting: {
      // CONNACK arrives via the packet parser; bail out on timeout.
      static uint32_t connectingSinceMs = 0;
      if (connectingSinceMs == 0) connectingSinceMs = now;
      if (!parseIncoming()) {
        if (now - connectingSinceMs > 5000) {
          connectingSinceMs = 0;
          scheduleReconnect();
        }
        return;
      }
      connectingSinceMs = 0;
      if (state != MqttState::Connected) scheduleReconnect();
      break;
    }

    case MqttState::Connected:
      if (!client.connected()) {
        scheduleReconnect();
        return;
      }
      parseIncoming();
      if (state != MqttState::Connected) break;
      republishChanged();
      if (now - lastDiagMs >= MQTT_DIAG_EVERY_MS) {
        lastDiagMs = now;
        for (size_t i = 0; i < registryCount(); ++i) {
          publishDiagnostics(registryGet(i));
        }
      }
      if (now - lastPingMs >= MQTT_PING_EVERY_MS) {
        lastPingMs = now;
        const uint8_t ping[] = {0xC0, 0x00};
        sendBytes(ping, sizeof(ping));
      }
      break;
  }
}

bool mqttConnected() {
  return state == MqttState::Connected;
}

bool mqttEnabledRuntime() {
  return runtimeEnabled;
}

void mqttSetEnabled(bool enabled) {
  runtimeEnabled = enabled;
  prefs.begin("mqtt", false);
  prefs.putBool("on", enabled);
  if (!enabled) {
    client.stop();
    state = MqttState::Disconnected;
    debugLogPrintln("MQTT: bridge disabled");
  } else {
    nextConnectAtMs = 0;
    debugLogPrintln("MQTT: bridge enabled");
  }
}

#else  // !MQTT_HOST: feature stubs so call sites stay unchanged.

void mqttBegin() {}
void mqttTick() {}
bool mqttConnected() { return false; }
bool mqttEnabledRuntime() { return false; }
void mqttSetEnabled(bool) {}

#endif  // MQTT_HOST
