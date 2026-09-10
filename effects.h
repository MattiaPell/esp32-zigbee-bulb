#pragma once

// ---------------------------------------------------------------------------
// effects: lightweight lighting effects that run on the coordinator.
//
//   color_loop  native ZCL color loop on each reachable bulb (color models)
//   candle      warm flicker: periodic collective white-temperature and
//               brightness steps with a short transition, so the bulbs
//               interpolate between them
//
// Only one effect runs at a time and it is runtime-only (lost on reboot, like
// the off-timers). Commands reuse the normal paths, so the registry state
// stays honest and the NVS debounce prevents repeated flash writes while the
// effect keeps updating.
// ---------------------------------------------------------------------------

#include <Arduino.h>

void effectsBegin();

// Runs the active effect's periodic work; call from the main loop.
void effectsTick();

// Starts an effect by id; "none" stops the current one. Returns false for an
// unknown id or when no reachable bulb can run it.
bool effectStart(const String &id);

void effectStop();

String effectActive();  // "none" when idle

// {"active":"...","effects":["candle","color_loop"]}
String effectsJson();
