#include "effects.h"

#include "config.h"
#include "debug_log.h"
#include "zigbee_bulbs.h"

namespace {

String active = "none";
uint32_t startedAtMs = 0;
uint32_t lastStepMs = 0;

constexpr uint32_t CANDLE_STEP_MS = 1200;      // Flicker step period
constexpr uint16_t CANDLE_TRANSITION_DS = 12;  // 1.2 s fade between steps
constexpr int CANDLE_KELVIN_SPAN = 400;        // MIN_KELVIN .. +400

// True when at least one reachable bulb is powered on.
bool aLightIsOn() {
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    if (bulbReady(b) && b->state.power) return true;
  }
  return false;
}

}  // namespace

void effectsBegin() {
  active = "none";
  startedAtMs = 0;
  lastStepMs = 0;
}

String effectActive() {
  return active;
}

void effectStop() {
  if (active == "color_loop") {
    for (size_t i = 0; i < registryCount(); ++i) {
      bulbSendColorLoop(registryGet(i), false);
    }
  }
  if (active != "none") {
    debugLogPrintf("Effects: stopped '%s'\n", active.c_str());
  }
  active = "none";
}

bool effectStart(const String &id) {
  if (id.length() == 0 || id.equalsIgnoreCase("none")) {
    effectStop();
    return true;
  }
  effectStop();

  if (id.equalsIgnoreCase("color_loop")) {
    int sent = 0;
    for (size_t i = 0; i < registryCount(); ++i) {
      Bulb *b = registryGet(i);
      if (!bulbReady(b)) continue;
      bulbSendColorLoop(b, true);
      ++sent;
    }
    if (sent == 0) return false;
  } else if (id.equalsIgnoreCase("candle")) {
    bool anyReady = false;
    for (size_t i = 0; i < registryCount(); ++i) {
      if (bulbReady(registryGet(i))) {
        anyReady = true;
        break;
      }
    }
    if (!anyReady) return false;
    randomSeed(millis());
  } else {
    return false;
  }

  active = id.equalsIgnoreCase("candle") ? "candle" : "color_loop";
  startedAtMs = millis();
  lastStepMs = 0;
  debugLogPrintf("Effects: started '%s'\n", active.c_str());
  return true;
}

void effectsTick() {
  if (active == "none") return;

  // A kill switch / all-off ends the effect. A short grace period lets the
  // candle turn the bulbs on with its first step before this is enforced.
  if (millis() - startedAtMs > 3000 && !aLightIsOn()) {
    effectStop();
    return;
  }

  if (active != "candle") return;

  const uint32_t now = millis();
  if (lastStepMs != 0 && now - lastStepMs < CANDLE_STEP_MS) return;
  lastStepMs = now;

  const int kelvin = MIN_KELVIN + (int)random(0, CANDLE_KELVIN_SPAN);
  const uint8_t pct = (uint8_t)(55 + random(0, 30));
  bulbSendAllKelvin(kelvin, CANDLE_TRANSITION_DS);
  bulbSendAllBrightness(pct, CANDLE_TRANSITION_DS);
}

String effectsJson() {
  String j = "{\"active\":\"";
  j += active;
  j += "\",\"effects\":[\"candle\",\"color_loop\"]}";
  return j;
}
