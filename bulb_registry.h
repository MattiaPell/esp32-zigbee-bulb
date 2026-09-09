#pragma once

// ---------------------------------------------------------------------------
// bulb_registry: the list of paired bulbs and their last known state.
//
// Identity is the bulb's IEEE address (stable across reboots and network
// address changes). Short addresses and endpoints are refreshed at runtime
// from the Zigbee binding table, because they may change.
//
// State is persisted in NVS (Preferences) per bulb and written with a
// debounce so rapid UI changes do not wear the flash.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include "Zigbee.h"
#include "config.h"

// How a bulb is currently being driven.
enum class BulbColorMode : uint8_t { White = 0, Rgb = 1 };

struct BulbState {
  BulbColorMode mode = BulbColorMode::White;
  bool power = false;
  uint8_t level = 128;  // Zigbee level 0-255 (DEFAULT_BRIGHTNESS_PCT maps here)
  uint16_t kelvin = DEFAULT_KELVIN;
  uint8_t red = 255;
  uint8_t green = 255;
  uint8_t blue = 255;
};

struct Bulb {
  esp_zb_ieee_addr_t ieee = {0};  // Persistent identity
  char name[24] = "Bulb";         // Editable via the web UI
  uint16_t shortAddr = 0xFFFF;    // Runtime only, refreshed from Zigbee
  uint8_t endpoint = 0;           // Runtime only, refreshed from Zigbee
  bool online = false;            // Bound and reachable
  bool groupMember = false;       // Runtime only: confirmed in the "all" group
  BulbState state;                // Last commanded/known state

  // --- Runtime diagnostics (not persisted) ----------------------------------
  uint32_t lastSeenMs = 0;        // Last report/readback from the bulb (millis); 0 = never
  uint8_t lqi = 0;                // Link quality from the NWK neighbor table (0-255)
  int8_t rssi = 0;                // RSSI from the NWK neighbor table (dBm), 0 = unknown
  uint16_t cmdSent = 0;           // Command operations attempted since boot
  uint16_t cmdFailed = 0;         // Operations rejected by a default response
  uint8_t lastFailStatus = 0xFF;  // Last failing ZCL status (0xFF = none)
};

// Load the registry from NVS. Call once at boot before Zigbee starts.
void registryBegin();

size_t registryCount();
Bulb *registryGet(size_t index);            // nullptr if out of range
Bulb *registryFindByIeee(const esp_zb_ieee_addr_t ieee);
Bulb *registryFindByIeeeHex(const String &ieeeHex);  // API id lookup (MQTT)

// Add with an auto name ("Bulb N"). Returns the new bulb, nullptr if full.
Bulb *registryAdd(const esp_zb_ieee_addr_t ieee);

// Remove a bulb and persist immediately.
void registryRemove(Bulb *bulb);

// Rename (sanitized) and persist immediately.
void registryRename(Bulb *bulb, const char *newName);

// Mark state dirty; a debounced write happens from the main loop.
void registryMarkDirty();

// Debounced NVS write of pending state changes; call from the main loop.
void registryTick();

// Write pending changes now (removals and renames flush immediately).
void registryFlush();

// Helpers shared by the web API.
void bulbSetBrightnessPct(Bulb *bulb, uint8_t pct);  // 0-100 -> level
uint8_t bulbBrightnessPct(const Bulb *bulb);         // level -> 0-100
String bulbIeeeHex(const Bulb *bulb);                // stable API id

// Compact state serialization ("mode,power,level,kelvin,r,g,b"), used by
// NVS storage and by the scenes module.
String serializeBulbState(const BulbState &state);
bool deserializeBulbState(BulbState &state, const String &text);
