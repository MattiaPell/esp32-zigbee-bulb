#pragma once

// ---------------------------------------------------------------------------
// light_timers: scheduled off-deadlines ("turn off in N minutes").
//
// Timers live in RAM only: they are intentionally lost on reboot (a power
// cut must not silently turn lights off hours later). One timer per bulb,
// identifiable by IEEE address (stable while registry slots shuffle), plus
// one global "all bulbs" timer identified by a null IEEE.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "bulb_registry.h"

void lightTimersBegin();
void lightTimersTick();

// seconds 0 clears the timer.
void bulbTimerSet(Bulb *bulb, uint32_t seconds);
uint32_t bulbTimerRemaining(const Bulb *bulb);  // 0 if inactive

void bulbTimerSetAll(uint32_t seconds);  // Ends with every bulb off.
uint32_t bulbTimerRemainingAll();
