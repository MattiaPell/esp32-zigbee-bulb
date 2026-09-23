#pragma once

#include <stdint.h>
#include "config.h"

// ---------------------------------------------------------------------------
// zigbee_utils.h: Shared utility functions for Zigbee components.
// ---------------------------------------------------------------------------

inline uint16_t clampTransition(uint16_t transitionDs) {
  if (transitionDs > MAX_TRANSITION_DS) return MAX_TRANSITION_DS;
  return transitionDs;
}
