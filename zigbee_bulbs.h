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

// True when the bulb is bound, addressable and able to receive commands.
bool bulbReady(const Bulb *bulb);

// --- Network members (diagnostics, read-only) -------------------------------

struct DeviceInfo {
  esp_zb_ieee_addr_t ieee = {0};
  uint16_t shortAddr = 0;
  uint8_t deviceType = 0;  // 0 coordinator, 1 router, 2 end device
};

// Refreshes the cached snapshot of Zigbee network members (neighbor table).
void zigbeeRequestMembers();

// Copies the last known snapshot; returns how many entries were written.
size_t zigbeeMemberSnapshot(DeviceInfo *out, size_t cap);

// Copies the bound devices list (grouped with the neighbor snapshot to
// include battery devices that leave the neighbor table while asleep).
size_t zigbeeBoundSnapshot(DeviceInfo *out, size_t cap);

// ZDO-unbinds the bulb (all clusters), drops it from the endpoint and the
// registry. Bulbs that are unreachable may re-appear on the next sync.
void zigbeeRemoveDevice(Bulb *bulb);

// Sends one attribute read per bulb (staggered); responses update the
// registry through the report callbacks. Used at boot and by the UI.
void zigbeeRefreshStates();

// Queues a ZDO Bind (remote -> bulb) for the steering clusters (on/off,
// level, color control). The bind requests are sent staggered from
// zigbeeTick (the remote is a sleepy device: several cycles give it time
// to pick them up while it polls). False if a job is already in flight
// or the parameters are unusable.
bool zigbeeQueueRemoteBind(const esp_zb_ieee_addr_t remoteIeee,
                           uint16_t remoteShort, uint8_t remoteEp,
                           const Bulb *bulb);
bool zigbeeRemoteBindBusy();

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

// Starts/stops the native ZCL color loop on the bulb (color models only).
void bulbSendColorLoop(Bulb *bulb, bool on);

// Kill switch: everything off, state persisted. Returns how many bulbs
// were commanded (group frame counts as every reachable bulb).
int bulbSendAllOff();

// Turns every reachable bulb back on, each at its own last state.
int bulbSendAllOn();

// Collective brightness / white temperature / color for every reachable
// bulb: one native group frame when possible, per-bulb unicast otherwise.
int bulbSendAllBrightness(uint8_t pct, uint16_t transitionDs);
int bulbSendAllKelvin(int kelvin, uint16_t transitionDs);
int bulbSendAllRgb(uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs);

// Resends the stored state of one bulb (used after boot / power cut).
void bulbSendFullState(Bulb *bulb);

// Drives one bulb to an arbitrary state snapshot: off, or on with the given
// level and white temperature / RGB color. Used by boot restore and scenes.
void bulbApplyState(Bulb *bulb, const struct BulbState &wanted, uint16_t transitionDs);
