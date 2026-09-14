#include "identify.h"

#include "debug_log.h"
#include "zigbee_bulbs.h"

namespace {

constexpr uint8_t IDENTIFY_BLINKS = 3;
constexpr uint32_t IDENTIFY_ON_MS = 500;
constexpr uint32_t IDENTIFY_OFF_MS = 400;

bool active = false;
esp_zb_ieee_addr_t ieee = {0};
uint8_t blinksLeft = 0;
bool lit = false;           // Current phase while blinking.
bool restorePower = false;  // Power state to restore at the end.
uint32_t nextStepMs = 0;

// Puts the bulb being blinked back to the power state it had when the
// sequence started, then stops. Used both on completion and on replacement.
void stopCurrent() {
  if (!active) return;
  Bulb *b = registryFindByIeee(ieee);
  if (b != nullptr && bulbReady(b) && b->state.power != restorePower) {
    restorePower ? bulbSendOn(b) : bulbSendOff(b);
  }
  active = false;
}

}  // namespace

void identifyBegin() {
  active = false;
  blinksLeft = 0;
  lit = false;
  restorePower = false;
  nextStepMs = 0;
}

bool identifyBulb(Bulb *bulb) {
  if (bulb == nullptr || !bulbReady(bulb)) return false;
  stopCurrent();  // Replace any blink already running.
  memcpy(ieee, bulb->ieee, sizeof(esp_zb_ieee_addr_t));
  restorePower = bulb->state.power;
  blinksLeft = IDENTIFY_BLINKS;
  lit = true;  // Light up immediately so the blink is visible.
  active = true;
  bulbSendOn(bulb);
  nextStepMs = millis() + IDENTIFY_ON_MS;
  debugLogPrintf("Identify: blinking %s\n", bulb->name);
  return true;
}

void identifyTick() {
  if (!active) return;
  if ((int32_t)(millis() - nextStepMs) < 0) return;

  Bulb *b = registryFindByIeee(ieee);
  if (b == nullptr || !bulbReady(b)) {
    active = false;  // Removed or dropped offline while blinking.
    return;
  }

  if (lit) {
    bulbSendOff(b);
    lit = false;
    nextStepMs = millis() + IDENTIFY_OFF_MS;
    return;
  }

  if (--blinksLeft == 0) {
    stopCurrent();  // Restore the original power state.
    debugLogPrintf("Identify: done for %s\n", b->name);
    return;
  }
  bulbSendOn(b);
  lit = true;
  nextStepMs = millis() + IDENTIFY_ON_MS;
}
