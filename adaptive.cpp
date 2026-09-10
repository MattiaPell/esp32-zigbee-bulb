#include "adaptive.h"

#include <Preferences.h>
#include <WiFi.h>
#include <time.h>

#include "adaptive_curve.h"
#include "config.h"
#include "debug_log.h"
#include "effects.h"
#include "zigbee_bulbs.h"

namespace {

Preferences prefs;
bool enabled = false;
int tzOffsetMin = 60;  // UTC+1 by default
bool timeConfigured = false;
uint32_t lastApplyMs = 0;

constexpr uint32_t ADAPTIVE_STEP_MS = 60000;  // Recompute once a minute
constexpr int ADAPTIVE_KELVIN_DELTA = 60;     // Skip tiny temperature changes
constexpr int ADAPTIVE_LEVEL_DELTA = 6;       // Skip tiny brightness changes (0-255)

bool timeSynced() {
  return time(nullptr) > 1700000000;  // A date after Nov 2023.
}

void configureTime() {
  configTime(tzOffsetMin * 60, 0, "pool.ntp.org", "time.cloudflare.com");
  timeConfigured = true;
}

}  // namespace

void adaptiveBegin() {
  prefs.begin("adaptive", false);
  enabled = prefs.getBool("on", false);
  tzOffsetMin = prefs.getInt("tz", 60);
}

bool adaptiveEnabled() {
  return enabled;
}

void adaptiveSetEnabled(bool on) {
  enabled = on;
  prefs.begin("adaptive", false);
  prefs.putBool("on", on);
  if (on) timeConfigured = false;  // (Re)start the NTP client on next tick.
}

int adaptiveTzOffsetMin() {
  return tzOffsetMin;
}

void adaptiveSetTzOffsetMin(int minutes) {
  if (minutes < -720 || minutes > 840) return;
  tzOffsetMin = minutes;
  prefs.begin("adaptive", false);
  prefs.putInt("tz", minutes);
  timeConfigured = false;
}

void adaptiveTick() {
  if (!enabled) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!timeConfigured) configureTime();
  if (!timeSynced()) return;
  if (effectActive() != "none") return;  // Manual effects take precedence.
  if (registryCount() == 0) return;

  const uint32_t now = millis();
  if (lastApplyMs != 0 && now - lastApplyMs < ADAPTIVE_STEP_MS) return;
  lastApplyMs = now;

  time_t t = time(nullptr);
  struct tm lt = {};
  localtime_r(&t, &lt);
  int kelvin = 0, brightness = 0;
  adaptiveAtMinute(lt.tm_hour * 60 + lt.tm_min, kelvin, brightness);

  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (!bulbReady(b) || !b->state.power) continue;
    if (b->state.mode == BulbColorMode::Rgb) continue;  // Leave color scenes alone.
    const int targetLevel = (brightness * 255 + 50) / 100;
    if (abs((int)b->state.kelvin - kelvin) < ADAPTIVE_KELVIN_DELTA &&
        abs((int)b->state.level - targetLevel) < ADAPTIVE_LEVEL_DELTA) {
      continue;
    }
    bulbSendKelvin(b, kelvin, DEFAULT_TRANSITION_DS);
    bulbSendBrightness(b, (uint8_t)brightness, DEFAULT_TRANSITION_DS);
    ++applied;
  }
  if (applied > 0) {
    debugLogPrintf("Adaptive: %02d:%02d -> %d K, %d%% (%d bulbs)\n", lt.tm_hour,
                  lt.tm_min, kelvin, brightness, applied);
  }
}

String adaptiveStatusJson() {
  String j = "{\"enabled\":";
  j += (enabled ? "true" : "false");
  j += ",\"synced\":";
  j += (timeSynced() ? "true" : "false");
  j += ",\"tz_offset_min\":";
  j += tzOffsetMin;
  if (timeSynced()) {
    time_t t = time(nullptr);
    struct tm lt = {};
    localtime_r(&t, &lt);
    int kelvin = 0, brightness = 0;
    adaptiveAtMinute(lt.tm_hour * 60 + lt.tm_min, kelvin, brightness);
    char hhmm[8];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", lt.tm_hour, lt.tm_min);
    j += ",\"local\":\"";
    j += hhmm;
    j += "\",\"kelvin\":";
    j += kelvin;
    j += ",\"brightness\":";
    j += brightness;
  }
  j += "}";
  return j;
}
