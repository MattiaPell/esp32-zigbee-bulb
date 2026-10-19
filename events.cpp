#include "events.h"

#include <WiFi.h>
#include <WiFiClient.h>

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "effects.h"
#include "light_timers.h"
#include "ota_update.h"
#include "scenes.h"
#include "sse.h"
#include "zigbee_bulbs.h"

namespace {

struct Pending {
  WiFiClient client;
  String head;
  uint32_t startMs = 0;
  bool used = false;
};

WiFiServer server(EVENTS_PORT);
bool serverUp = false;
Pending pending[EVENTS_MAX_PENDING];
WiFiClient streams[EVENTS_MAX_CLIENTS];

uint32_t version = 1;
uint32_t lastSignature = 0;
uint32_t lastCheckMs = 0;
uint32_t lastBeatMs = 0;

// Hash of everything the page shows that is not continuous. Timer countdowns
// are left out (the page keeps a slow poll for them) and, while an effect
// runs, so are the levels it rewrites several times a second.
uint32_t stateSignature() {
  uint32_t h = FNV1A_SEED;
  const String effect = effectActive();
  const bool effectRunning = effect != "none";
  h = fnv1aAdd(h, effect.c_str(), effect.length());
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    const uint8_t flags = (uint8_t)((b->online ? 1 : 0) | (b->state.power ? 2 : 0) |
                                    (bulbTimerRemaining(b) > 0 ? 4 : 0));
    h = fnv1aAdd(h, b->ieee, sizeof(b->ieee));
    h = fnv1aAdd(h, b->name, strlen(b->name));
    h = fnv1aAdd(h, &flags, 1);
    if (effectRunning) continue;
    const uint8_t level = bulbBrightnessPct(b);
    const uint8_t mode = (uint8_t)b->state.mode;
    const uint16_t kelvin = b->state.kelvin;
    h = fnv1aAdd(h, &level, 1);
    h = fnv1aAdd(h, &mode, 1);
    h = fnv1aAdd(h, &kelvin, sizeof(kelvin));
    h = fnv1aAdd(h, &b->state.red, 1);
    h = fnv1aAdd(h, &b->state.green, 1);
    h = fnv1aAdd(h, &b->state.blue, 1);
  }
  const uint8_t misc[] = {(uint8_t)scenesCount(), (uint8_t)zigbeePairingActive(),
                          (uint8_t)otaPendingVerify(),
                          (uint8_t)(bulbTimerRemainingAll() > 0)};
  return fnv1aAdd(h, misc, sizeof(misc));
}

bool writeAll(WiFiClient &c, const String &s) {
  return c.write((const uint8_t *)s.c_str(), s.length()) == s.length();
}

void promote(Pending &p) {
  const SseRequest req = parseSseRequest(p.head);
  const String ip = WiFi.localIP().toString();
  if (!sseRequestAllowed(req, storedHostname(), ip)) {
    p.client.print("HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n");
    p.client.stop();
    return;
  }
  for (WiFiClient &s : streams) {
    if (s.connected()) continue;
    s = p.client;
    String out = sseResponseHead(req.origin);
    out += sseRetryFrame();
    out += sseEventFrame(version);
    if (!writeAll(s, out)) s.stop();
    return;
  }
  p.client.print("HTTP/1.1 503 Service Unavailable\r\nConnection: close\r\n\r\n");
  p.client.stop();
}

void acceptClients() {
  WiFiClient c = server.accept();
  if (!c) return;
  for (Pending &p : pending) {
    if (p.used) continue;
    p.client = c;
    p.head = "";
    p.startMs = millis();
    p.used = true;
    return;
  }
  c.stop();  // Too many half-open requests.
}

void readPending() {
  for (Pending &p : pending) {
    if (!p.used) continue;
    while (p.client.available() && p.head.length() < EVENTS_MAX_HEAD) {
      p.head += (char)p.client.read();
    }
    const bool complete = p.head.indexOf("\r\n\r\n") >= 0;
    const bool expired = millis() - p.startMs > EVENTS_HANDSHAKE_MS ||
                         p.head.length() >= EVENTS_MAX_HEAD || !p.client.connected();
    if (complete) {
      promote(p);
    } else if (expired) {
      p.client.stop();
    } else {
      continue;
    }
    p.used = false;
    p.head = "";
  }
}

void broadcast(const String &frame) {
  for (WiFiClient &s : streams) {
    if (!s.connected()) continue;
    if (!writeAll(s, frame)) s.stop();
  }
}

}  // namespace

void eventsBegin() {}

void eventsTick() {
  if (WiFi.status() != WL_CONNECTED) {
    serverUp = false;
    return;
  }
  if (!serverUp) {
    server.begin();
    serverUp = true;
    lastSignature = stateSignature();
    debugLogPrintf("Events: SSE stream on port %u\n", (unsigned)EVENTS_PORT);
  }
  acceptClients();
  readPending();

  const uint32_t now = millis();
  if (now - lastCheckMs >= EVENTS_CHECK_MS) {
    lastCheckMs = now;
    const uint32_t sig = stateSignature();
    if (sig != lastSignature) {
      lastSignature = sig;
      ++version;
      broadcast(sseEventFrame(version));
      lastBeatMs = now;
    }
  }
  if (now - lastBeatMs >= EVENTS_HEARTBEAT_MS) {
    lastBeatMs = now;
    broadcast(sseHeartbeatFrame());
  }
}

size_t eventsClientCount() {
  size_t n = 0;
  for (WiFiClient &s : streams) {
    if (s.connected()) ++n;
  }
  return n;
}
