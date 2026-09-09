#pragma once

// ---------------------------------------------------------------------------
// zigbee_groups: native Zigbee group addressing (Groups cluster 0x0004).
//
// All registered bulbs share one group ("all lights"). Membership is pushed
// to bulbs with Add Group commands; bulbs persist it in their own memory and
// re-enrollment is idempotent. Collective commands then travel as ONE
// group-addressed frame instead of one frame per bulb.
//
// The fallback to the per-bulb loop (zigbee_bulbs) happens whenever some
// reachable bulb is not a confirmed member yet: enrollment is optimistic
// (flag set after the command is accepted by the stack) and re-sent on every
// offline -> online transition, so a bulb that missed it eventually joins.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "bulb_registry.h"

// Queue group enrollment for a bulb. False if the queue is full (the next
// online transition retries).
bool groupEnroll(Bulb *bulb);

// One staggered enrollment step (max one Add Group per interval); call from
// the Zigbee tick.
void groupTick(uint32_t now);

// Fire-and-forget Remove Group for a bulb that is being forgotten.
void groupForget(Bulb *bulb);

// --- Group-addressed sends ---------------------------------------------------
//
// Low-level "one frame for the whole group" sends. The caller checks group
// usability (every reachable bulb a member) and updates the registry.

bool groupSendOn();
bool groupSendOff();
bool groupSendBrightness(uint8_t pct, uint16_t transitionDs);
bool groupSendKelvin(int kelvin, uint16_t transitionDs);
bool groupSendRgb(uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs);
