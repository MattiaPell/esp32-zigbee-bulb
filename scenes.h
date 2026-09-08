#pragma once

// ---------------------------------------------------------------------------
// scenes: named snapshots of every bulb's state.
//
// A scene stores, for each bulb registered at capture time, its full state
// (power, level, white temperature or RGB). Recalling re-sends those states;
// bulbs that left the registry since are skipped.
//
// Names are restricted to a-z / 0-9 / '-' / '_' (they appear in URL paths).
// Storage: NVS namespace "scenes" (separate from the bulb registry).
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <WString.h>

#include "bulb_registry.h"

constexpr uint8_t MAX_SCENES = 16;
constexpr size_t MAX_SCENE_NAME_LENGTH = 20;

void scenesBegin();

size_t scenesCount();
String sceneNameAt(size_t index);  // "" if out of range
int sceneIndexOf(const String &name);

// Captures the current state of all registered bulbs under the given name
// (creating or replacing). Returns false if invalid name or storage full.
bool sceneCapture(const String &name);

// Applies the stored states. Writes how many bulbs were commanded.
bool sceneRecall(const String &name, int &applied, int &skipped);

void sceneDelete(const String &name);

bool isValidSceneName(const String &name);
