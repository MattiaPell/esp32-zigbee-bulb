#pragma once

// ---------------------------------------------------------------------------
// zigbee_bulbs: the Zigbee coordinator side.
//
// One color-dimmer-switch endpoint binds to every IKEA bulb that joins the
// network (multi-binding). Each bulb is addressed by short address +
// endpoint; both are refreshed at runtime because they can change:
//
//   - After boot, short addresses are resolved from IEEE addresses with a
//     ZDO NWK-address request (direct IEEE-addressed commands are known to
//     be unreliable with IKEA bulbs).
//   - Attribute reports are routed back to bulbs by source address; an
//     unknown source is resolved with a ZDO IEEE-address request.
//
// The web layer calls only the commands here; state updates arrive through
// the report callbacks and land in the registry.
// ---------------------------------------------------------------------------

#include <Arduino.h>

#include "bulb_registry.h"
#include "config.h"

// Configure the endpoint, register callbacks and start the coordinator.
// Restarts the board when the Zigbee stack cannot be brought up.
void zigbeeBegin();

// Periodic work: registry/binding sync, address resolution, boot state
// resend, automatic pairing window while no bulb is bound.
void zigbeeTick();

void zigbeeOpenPairing(uint8_t seconds);
bool zigbeePairingActive();
size_t zigbeeBoundDeviceCount();

// ZDO-unbinds the bulb (all clusters), drops it from the endpoint and the
// registry. Bulbs that are unreachable may re-appear on the next sync.
void zigbeeRemoveDevice(Bulb *bulb);

// Sends one attribute read per bulb (staggered); responses update the
// registry through the report callbacks. Used at boot and by the UI.
void zigbeeRefreshStates();

// --- Per-bulb commands (no-ops for offline/unresolved bulbs) ---------------

void bulbSendOn(Bulb *bulb);
void bulbSendOff(Bulb *bulb);
void bulbSendToggle(Bulb *bulb);

// 0-100 %; 0 % sends off. Fades with the given transition (0.1 s units).
void bulbSendBrightness(Bulb *bulb, uint8_t pct, uint16_t transitionDs);

// White temperature in kelvin (converted to mireds).
void bulbSendKelvin(Bulb *bulb, int kelvin, uint16_t transitionDs);

// RGB color, sent as CIE XY (converted with the library helper).
void bulbSendRgb(Bulb *bulb, uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs);

// Kill switch: everything off, state persisted.
void bulbSendAllOff();

// Resends the stored state of one bulb (used after boot / power cut).
void bulbSendFullState(Bulb *bulb);
