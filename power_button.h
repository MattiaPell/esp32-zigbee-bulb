#pragma once

// BOOT button as a physical kill switch: a clean press turns every bulb off.
// Setup: pinMode INPUT_PULLUP is handled here; call powerButtonBegin() with
// the callback and powerButtonTick() from the main loop.

#include <Arduino.h>

void powerButtonBegin(void (*onKillSwitch)());
void powerButtonTick();
