#pragma once

// ---------------------------------------------------------------------------
// remote_controls: IKEA Zigbee remotes / steering devices as triggers.
//
// Remotes bind to the coordinator endpoint like bulbs do, but expose on/off
// as an OUTPUT (client) cluster: zigbee_bulbs enrolls them here instead of
// rejecting them. Their button presses arrive as incoming ZCL commands that
// the stack would drop (On/Off, Level Move/Step/Stop, Color steps, Scene
// recall); they are intercepted with privilege commands and normalized into
// press events mapped to actions (default: toggle = master switch, ring /
// steps = dim all lights, arrows = previous/next scene, scene recall = that
// scene).
//
// V1 notes: the event->action map is global (one row per event type,
// persisted). The action map only drives the mapped events; a Scenes Recall
// always recalls the scene with that id (capped to the saved list).
// ---------------------------------------------------------------------------

#include <Arduino.h>

#include "bulb_registry.h"

class ZigbeeEP;

// Registers the privilege-command interception set and the callbacks on the
// coordinator endpoint. Call after the Zigbee stack is up.
void remoteAttach(ZigbeeEP &ep);

void remotesBegin();

// Housekeeping: IEEE resolution for newly seen remotes (staggered).
void remotesTick();

// Enrolls a bound device recognized as a remote/steering device (on/off as
// an OUTPUT cluster). False if the registry is full.
bool remoteEnroll(const esp_zb_ieee_addr_t ieee, uint16_t shortAddr,
                  uint8_t endpoint);

// REST-facing serialization.
String remoteListJson();

size_t remoteCount();
String remoteIdAt(size_t index);  // IEEE hex, "" if out of range

// Rename and persist. False if unknown.
bool remoteRename(const String &id, const char *newName);

// Forgets the remote (its own binding lives in the device: factory-reset the
// remote to re-enroll). False if unknown.
bool remoteRemove(const String &id);

// --- Action map (global, persisted) -----------------------------------------

// Overrides the map with event=action pairs ("toggle":"toggle_all", ...).
// Unknown pairs are rejected; at least one valid pair required.
bool remoteSetActions(const String &body, String &applied);

String remoteActionsJson();
