#include "light_timers.h"

#include "bulb_registry.h"
#include "debug_log.h"
#include "web_hooks.h"
#include "zigbee_bulbs.h"

namespace {

constexpr size_t SLOTS = 1 + MAX_BULBS;   // One extra: global "all bulbs" timer.
constexpr uint32_t TIMER_LIMIT_S = 24UL * 60 * 60;
const esp_zb_ieee_addr_t kZeroIeee = {0, 0, 0, 0, 0, 0, 0, 0};

struct TimerEntry {
  bool active = false;
  esp_zb_ieee_addr_t ieee = {0};  // All-zero = the "all bulbs" timer.
  uint32_t expireMs = 0;
};

TimerEntry entries[SLOTS];

bool ieeeSame(const esp_zb_ieee_addr_t a, const esp_zb_ieee_addr_t b) {
  return memcmp(a, b, sizeof(esp_zb_ieee_addr_t)) == 0;
}

// Finds the active slot for these identifiers, or (create=true) a free one.
int findSlot(const esp_zb_ieee_addr_t ieee, bool create) {
  int freeSlot = -1;
  for (size_t i = 0; i < SLOTS; ++i) {
    if (entries[i].active && ieeeSame(entries[i].ieee, ieee)) return (int)i;
    if (!entries[i].active && freeSlot < 0) freeSlot = (int)i;
  }
  return create ? freeSlot : -1;
}

void clearSlot(int slot) {
  entries[slot].active = false;
}

void setSlot(int slot, const esp_zb_ieee_addr_t ieee, uint32_t seconds) {
  if (seconds > TIMER_LIMIT_S) seconds = TIMER_LIMIT_S;
  memcpy(entries[slot].ieee, ieee, sizeof(esp_zb_ieee_addr_t));
  entries[slot].active = true;
  entries[slot].expireMs = millis() + seconds * 1000UL;
  debugLogPrintf("Timers: off in %lu s (slot %d)\n", (unsigned long)seconds, slot);
}

}  // namespace

void lightTimersBegin() {
  // RAM-only state: nothing to load, nothing to save.
}

void lightTimersTick() {
  const uint32_t now = millis();
  for (size_t i = 0; i < SLOTS; ++i) {
    if (!entries[i].active) continue;
    if ((int32_t)(now - entries[i].expireMs) >= 0) {
      if (ieeeSame(entries[i].ieee, kZeroIeee)) {
        webHookEvent("timer_expired", "", "all bulbs", "all");
        bulbSendAllOff();
      } else {
        Bulb *b = registryFindByIeee(entries[i].ieee);
        if (b != nullptr) {
          webHookEvent("timer_expired", bulbIeeeHex(b).c_str(), b->name);
          bulbSendOff(b);
        }
      }
      entries[i].active = false;
      debugLogPrintln("Timers: expired, bulbs off.");
    }
  }
}

void bulbTimerSet(Bulb *bulb, uint32_t seconds) {
  if (bulb == nullptr) return;
  int slot = findSlot(bulb->ieee, seconds > 0);
  if (seconds == 0) {
    if (slot >= 0) {
      entries[slot].active = false;
      debugLogPrintf("Timers: cancelled for %s\n", bulb->name);
    }
    return;
  }
  if (slot < 0) {
    debugLogPrintln("Timers: no free slot");
    return;
  }
  setSlot(slot, bulb->ieee, seconds);
}

uint32_t bulbTimerRemaining(const Bulb *bulb) {
  if (bulb == nullptr) return 0;
  const int slot = findSlot(bulb->ieee, false);
  if (slot < 0) return 0;
  const int32_t leftMs = (int32_t)(entries[slot].expireMs - millis());
  return leftMs > 0 ? (uint32_t)leftMs / 1000 : 0;
}

void bulbTimerSetAll(uint32_t seconds) {
  int slot = findSlot(kZeroIeee, seconds > 0);
  if (seconds == 0) {
    if (slot >= 0) {
      entries[slot].active = false;
      debugLogPrintln("Timers: global timer cancelled");
    }
    return;
  }
  if (slot < 0) {
    debugLogPrintln("Timers: no free slot");
    return;
  }
  setSlot(slot, kZeroIeee, seconds);
}

uint32_t bulbTimerRemainingAll() {
  const int slot = findSlot(kZeroIeee, false);
  if (slot < 0) return 0;
  const int32_t leftMs = (int32_t)(entries[slot].expireMs - millis());
  return leftMs > 0 ? (uint32_t)leftMs / 1000 : 0;
}
