#pragma once

// ---------------------------------------------------------------------------
// adaptive: follow the time of day (NTP + a fixed UTC offset) with a warm,
// dim white at night and a cool, bright white at noon. It only adjusts bulbs
// that are already on and in white mode; RGB scenes and running effects are
// left alone. Runtime state (enabled, UTC offset) is persisted in NVS.
//
// The curve itself lives in adaptive_curve.h (pure, unit-tested).
// ---------------------------------------------------------------------------

#include <Arduino.h>

void adaptiveBegin();

// Recomputes and applies the target once a minute; call from the main loop.
void adaptiveTick();

bool adaptiveEnabled();
void adaptiveSetEnabled(bool on);

int adaptiveTzOffsetMin();  // minutes from UTC (e.g. 60 = UTC+1)
void adaptiveSetTzOffsetMin(int minutes);

// {"enabled":..,"synced":..,"tz_offset_min":..,"local":"HH:MM",
//  "kelvin":..,"brightness":..}  (time fields only once NTP has synced)
String adaptiveStatusJson();
