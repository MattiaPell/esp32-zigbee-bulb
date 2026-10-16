#pragma once

#include <stdint.h>
#include "config.h"
#include <string.h>
#include "Zigbee.h"

// ---------------------------------------------------------------------------
// zigbee_utils.h: Shared utility functions for Zigbee components.
// ---------------------------------------------------------------------------

inline uint16_t clampTransition(uint16_t transitionDs) {
  if (transitionDs > MAX_TRANSITION_DS) return MAX_TRANSITION_DS;
  return transitionDs;
}

inline bool ieeeListContains(const esp_zb_ieee_addr_t list[], size_t count, const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < count; ++i) {
    if (memcmp(list[i], ieee, sizeof(esp_zb_ieee_addr_t)) == 0) return true;
  }
  return false;
}
