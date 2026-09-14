#pragma once

// ---------------------------------------------------------------------------
// identify: blink one bulb a few times to locate it physically.
//
// The blink runs from the main loop (a sequence of on/off commands with a
// short period) so the web handler returns immediately. The bulb's original
// power state is restored when the sequence ends. Only one bulb is blinked
// at a time; a new request replaces the running one.
// ---------------------------------------------------------------------------

#include <Arduino.h>

#include "bulb_registry.h"

void identifyBegin();

// Runs the blink sequence; call from the main loop.
void identifyTick();

// Starts blinking the given bulb. False when it is offline/unaddressable.
bool identifyBulb(Bulb *bulb);
