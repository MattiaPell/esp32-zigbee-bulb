#include "power_button.h"

#include "config.h"

namespace {

constexpr uint32_t DEBOUNCE_MS = 40;

void (*killSwitchCallback)() = nullptr;
bool stableState = HIGH;      // Debounced level (HIGH = released)
bool lastRawState = HIGH;
uint32_t lastChangeMs = 0;
bool pressedHandled = false;

}  // namespace

void powerButtonBegin(void (*onKillSwitch)()) {
  killSwitchCallback = onKillSwitch;
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  lastRawState = digitalRead(POWER_BUTTON_PIN);
  stableState = lastRawState;
  lastChangeMs = millis();
}

void powerButtonTick() {
  const bool raw = digitalRead(POWER_BUTTON_PIN);
  const uint32_t now = millis();

  if (raw != lastRawState) {
    lastRawState = raw;
    lastChangeMs = now;
  }

  if (raw == stableState) {
    // Released after a handled press: re-arm for the next press.
    if (stableState == LOW && !pressedHandled) return;
    if (stableState == LOW && now - lastChangeMs > 200) pressedHandled = false;
    return;
  }

  if (now - lastChangeMs < DEBOUNCE_MS) return;

  stableState = raw;
  if (stableState == LOW && !pressedHandled) {
    pressedHandled = true;
    if (killSwitchCallback != nullptr) killSwitchCallback();
  }
}
