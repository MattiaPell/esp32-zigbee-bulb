#include "remote_controls.h"

#include <Preferences.h>
#include <Zigbee.h>

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "json_lite.h"
#include "scenes.h"
#include "web_hooks.h"
#include "zigbee_bulbs.h"

namespace {

// --- Events / actions ---------------------------------------------------------

enum RemoteEvent : uint8_t {
  EV_OFF = 0,
  EV_ON,
  EV_TOGGLE,
  EV_MOVE_UP,
  EV_MOVE_DOWN,
  EV_STOP,
  EV_STEP_UP,
  EV_STEP_DOWN,
  EV_COLOR_A,  // Color step/temp step, direction 0 (e.g. right / warmer)
  EV_COLOR_B,  // Color step/temp step, direction 1 (e.g. left / cooler)
  EV_SCENE,    // Scenes cluster Recall (mapped by payload, not by the table)
  EV_COUNT,
};

enum RemoteAction : uint8_t {
  ACT_NONE = 0,
  ACT_ALL_ON,
  ACT_ALL_OFF,
  ACT_TOGGLE_ALL,
  ACT_DIM_UP,
  ACT_DIM_DOWN,
  ACT_SCENE_NEXT,
  ACT_SCENE_PREV,
  ACT_COUNT,
};

const char *eventName(uint8_t ev) {
  static const char *NAMES[] = {"off",     "on",    "toggle",      "move_up",
                                "move_down", "stop", "step_up",   "step_down",
                                "color_a", "color_b", "scene"};
  return ev < EV_COUNT ? NAMES[ev] : "?";
}

const char *actionName(uint8_t act) {
  static const char *NAMES[] = {"none",        "all_on",     "all_off",
                                "toggle_all", "brightness_up", "brightness_down",
                                "scene_next", "scene_prev"};
  return act < ACT_COUNT ? NAMES[act] : "none";
}

bool actionFromName(const char *name, uint8_t &out) {
  for (uint8_t a = 0; a < ACT_COUNT; ++a) {
    if (strcmp(name, actionName(a)) == 0) {
      out = a;
      return true;
    }
  }
  return false;
}

// Defaults: the remote works usefully with zero configuration.
uint8_t actionMap[EV_COUNT] = {
    ACT_ALL_OFF,     // off
    ACT_ALL_ON,      // on
    ACT_TOGGLE_ALL,  // toggle
    ACT_DIM_UP,      // move_up (repeated while the ring is held)
    ACT_DIM_DOWN,    // move_down
    ACT_NONE,        // stop (ring release)
    ACT_DIM_UP,      // step_up
    ACT_DIM_DOWN,    // step_down
    ACT_SCENE_NEXT,  // color_a
    ACT_SCENE_PREV,  // color_b
    ACT_NONE,        // scene (payload-driven)
};

// --- Registry ------------------------------------------------------------------

struct Remote {
  bool used = false;
  esp_zb_ieee_addr_t ieee = {0};  // Persistent identity (0 until resolved)
  char name[24] = "Remote";
  uint16_t shortAddr = 0xFFFF;  // Runtime: refreshed by every press
  uint8_t endpoint = 0;         // Runtime
  uint32_t lastSeenMs = 0;      // Runtime
  char lastEvent[12] = "";      // Runtime
  uint32_t lastHookMs = 0;      // Runtime: webhook dedupe window
  char lastHookEvent[12] = "";
};

constexpr const char *PREFS_NS = "remotes";

Remote remotes[MAX_REMOTES];
Preferences prefs;

uint8_t sceneCursor = 0;   // Scene cycling position (RAM only)
bool ieeePending = false;  // One ZDO IEEE lookup in flight
uint32_t lastResolveMs = 0;

String ieeeHexOf(const esp_zb_ieee_addr_t ieee) {
  Bulb scratch;
  memcpy(scratch.ieee, ieee, sizeof(esp_zb_ieee_addr_t));
  return bulbIeeeHex(&scratch);
}

Remote *findByShort(uint16_t shortAddr) {
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (remotes[i].used && remotes[i].shortAddr == shortAddr) return &remotes[i];
  }
  return nullptr;
}

Remote *findByIeee(const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (remotes[i].used &&
        memcmp(remotes[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
      return &remotes[i];
    }
  }
  return nullptr;
}

Remote *findByApiId(const String &id) {
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (remotes[i].used && ieeeHexOf(remotes[i].ieee).equalsIgnoreCase(id)) {
      return &remotes[i];
    }
  }
  return nullptr;
}

Remote *freeSlot() {
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (!remotes[i].used) return &remotes[i];
  }
  return nullptr;
}

void persistRemote(Remote &r, int slot) {
  String key = "r" + String(slot);
  prefs.putBytes((key + "i").c_str(), r.ieee, sizeof(esp_zb_ieee_addr_t));
  prefs.putString((key + "n").c_str(), r.name);
}

void persistCount() {
  uint8_t n = 0;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (remotes[i].used) ++n;
  }
  prefs.putUChar("count", n);
}

void persistMap() {
  String csv;
  for (uint8_t ev = 0; ev < EV_COUNT; ++ev) {
    if (ev > 0) csv += ",";
    csv += String(actionMap[ev]);
  }
  prefs.putString("m", csv);
}

// --- ZDO IEEE resolution (staggered, one at a time) -----------------------------

void resolveNextRemote() {
  if (ieeePending) return;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    Remote &r = remotes[i];
    if (!r.used || r.shortAddr == 0xFFFF) continue;
    const bool hasIeee = r.ieee[0] != 0 || r.ieee[7] != 0;
    if (hasIeee) continue;

    esp_zb_zdo_ieee_addr_req_param_t req = {};  // Copied synchronously.
    req.dst_nwk_addr = 0xFFFC;  // Broadcast: the device answers for itself.
    req.addr_of_interest = r.shortAddr;
    req.request_type = 0;
    req.start_index = 0;
    ieeePending = true;
    if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
    esp_zb_zdo_ieee_addr_req(&req, [](esp_zb_zdp_status_t status,
                                      esp_zb_zdo_ieee_addr_rsp_t *resp, void *) {
      ieeePending = false;
      if (status != ESP_ZB_ZDP_STATUS_SUCCESS || resp == nullptr) return;
      Remote *match = nullptr;
      for (size_t i = 0; i < MAX_REMOTES; ++i) {
        if (remotes[i].used && remotes[i].ieee[0] == 0 && remotes[i].ieee[7] == 0 &&
            remotes[i].shortAddr == resp->nwk_addr) {
          match = &remotes[i];
          break;
        }
      }
      if (match == nullptr) return;
      // Merge guard: a rejoin creates a second slot for the same remote
      // (funny new short address). Prefer the already-named slot and free
      // the duplicate instead of keeping two entries for one device.
      Remote *older = nullptr;
      for (size_t i = 0; i < MAX_REMOTES; ++i) {
        if (remotes[i].used && &remotes[i] != match &&
            memcmp(remotes[i].ieee, resp->ieee_addr,
                   sizeof(esp_zb_ieee_addr_t)) == 0) {
          older = &remotes[i];
          break;
        }
      }
      if (older != nullptr) {
        older->shortAddr = resp->nwk_addr;
        debugLogPrintf("Remote: merged %s onto new short 0x%04x\n", older->name,
                      resp->nwk_addr);
        match->used = false;
        persistCount();
        return;
      }
      memcpy(match->ieee, resp->ieee_addr, sizeof(esp_zb_ieee_addr_t));
      for (size_t i = 0; i < MAX_REMOTES; ++i) {
        if (&remotes[i] == match) {
          persistRemote(remotes[i], (int)i);
          persistCount();
          break;
        }
      }
      debugLogPrintf("Remote: registered %s (%s)\n", match->name,
                    ieeeHexOf(match->ieee).c_str());
    }, nullptr);
    esp_zb_lock_release();
    return;
  }
}

// --- Actions ---------------------------------------------------------------------

bool anyBulbOn() {
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (bulbReady(b) && b->state.power) return true;
  }
  return false;
}

void executeAction(uint8_t action, const Remote &r) {
  switch (action) {
    case ACT_ALL_ON:
      bulbSendAllOn();
      break;
    case ACT_ALL_OFF:
      bulbSendAllOff();
      break;
    case ACT_TOGGLE_ALL:
      anyBulbOn() ? bulbSendAllOff() : bulbSendAllOn();
      break;
    case ACT_DIM_UP:
    case ACT_DIM_DOWN: {
      const int delta = action == ACT_DIM_UP ? REMOTE_DIM_STEP_PCT
                                             : -(int)REMOTE_DIM_STEP_PCT;
      for (size_t i = 0; i < registryCount(); ++i) {
        Bulb *b = registryGet(i);
        if (!bulbReady(b)) continue;
        const int pct =
            constrain((int)bulbBrightnessPct(b) + delta, 1, 100);
        bulbSendBrightness(b, (uint8_t)pct, DEFAULT_TRANSITION_DS);
      }
      break;
    }
    case ACT_SCENE_NEXT:
    case ACT_SCENE_PREV: {
      const int count = (int)scenesCount();
      if (count == 0) break;
      const int step = action == ACT_SCENE_NEXT ? 1 : count - 1;
      sceneCursor = (uint8_t)((sceneCursor + step) % count);
      int applied = 0, skipped = 0;
      sceneRecall(sceneNameAt(sceneCursor), applied, skipped);
      break;
    }
    default:
      break;
  }
}

// --- Command normalization ---------------------------------------------------------

RemoteEvent normalizeEvent(uint16_t cluster, uint8_t cmd, const uint8_t *data,
                           size_t len) {
  const uint8_t mode = len >= 1 ? data[0] : 0;
  switch (cluster) {
    case 0x0006:  // On/Off: off=0x00, on=0x01, toggle=0x02
      if (cmd == 0x00) return EV_OFF;
      if (cmd == 0x01) return EV_ON;
      if (cmd == 0x02) return EV_TOGGLE;
      break;
    case 0x0008:  // Level control
      if (cmd == 0x01 || cmd == 0x05) {  // Move / MoveWithOnOff
        return mode == 0 ? EV_MOVE_UP : EV_MOVE_DOWN;
      }
      if (cmd == 0x02 || cmd == 0x04) {  // Step / StepWithOnOff
        return mode == 0 ? EV_STEP_UP : EV_STEP_DOWN;
      }
      if (cmd == 0x03 || cmd == 0x06) {  // Stop / StopWithOnOff
        return EV_STOP;
      }
      break;
    case 0x0300:  // Color control: hue/saturation/temperature steps
      switch (cmd) {
        case 0x01:  // MoveHue
        case 0x02:  // StepHue
        case 0x04:  // MoveSaturation
        case 0x05:  // StepSaturation
        case 0x4B:  // MoveColorTemperature
        case 0x4C:  // StepColorTemperature
        case 0x4D:  // StepColorTemperatureWithOnOff
          return mode == 0 ? EV_COLOR_A : EV_COLOR_B;
      }
      break;
    case 0x0005:  // Scenes
      if (cmd == 0x05) return EV_SCENE;  // Recall
      break;
  }
  return EV_COUNT;  // Not normalized (logged as unknown).
}

// The Scenes Recall payload carries the scene id: recall that scene.
void recallSceneById(const uint8_t *data, size_t len) {
  if (len < 3) return;
  const uint8_t sceneId = data[2];
  if (sceneId == 0xFF || sceneId >= scenesCount()) return;
  sceneCursor = sceneId;
  int applied = 0, skipped = 0;
  sceneRecall(sceneNameAt(sceneId), applied, skipped);
}

// --- Hook callbacks ------------------------------------------------------------------

// Deferred bind: requested while the remote was asleep; fires when the
// remote is finally seen with a known short address.
struct PendingBind {
  bool used = false;
  esp_zb_ieee_addr_t remoteIeee = {0};
  Bulb bulb;
  uint8_t endpoint = 1;
};
PendingBind pendingBind;

bool armBindOnFirstPress(const esp_zb_ieee_addr_t ieee, const Bulb &bulb,
                         uint8_t endpoint) {
  pendingBind = PendingBind();
  pendingBind.used = true;
  memcpy(pendingBind.remoteIeee, ieee, sizeof(esp_zb_ieee_addr_t));
  pendingBind.bulb = bulb;
  pendingBind.endpoint = endpoint;
  debugLogPrintf(
      "Remote: bind armed for %s -> %s; press a key to complete it.\n",
      ieeeHexOf(ieee).c_str(), bulb.name);
  return true;
}

// Fired from remotesTick: converts the deferred request into a queued
// zigbee bind job once the remote's short address is known.
void flushPendingBind() {
  if (!pendingBind.used) return;
  uint16_t shortAddr = 0xFFFF;
  DeviceInfo net[14];
  size_t n = zigbeeMemberSnapshot(net, 7);
  n += zigbeeBoundSnapshot(net + n, 14 - n);
  for (size_t i = 0; i < n; ++i) {
    if (net[i].ieee[0] == 0 && net[i].ieee[7] == 0) continue;
    if (memcmp(net[i].ieee, pendingBind.remoteIeee,
               sizeof(esp_zb_ieee_addr_t)) == 0 &&
        net[i].shortAddr != 0xFFFF && net[i].shortAddr != 0) {
      shortAddr = net[i].shortAddr;
      break;
    }
  }
  if (shortAddr == 0xFFFF) {
    // Not in the live snapshots: fall back to the registry entry; a press
    // refreshes it through the duplicate-merge path.
    Remote *r = findByIeee(pendingBind.remoteIeee);
    if (r != nullptr && r->shortAddr != 0xFFFF && r->shortAddr != 0)
      shortAddr = r->shortAddr;
    if (shortAddr == 0xFFFF) return;  // Still asleep/unknown.
  }
  if (!zigbeeQueueRemoteBind(pendingBind.remoteIeee, shortAddr,
                             pendingBind.endpoint, &pendingBind.bulb)) {
    return;  // A job is already in flight; retry on a later tick.
  }
  Remote *r = findByIeee(pendingBind.remoteIeee);
  if (r != nullptr) r->endpoint = pendingBind.endpoint;
  pendingBind.used = false;
  debugLogPrintf("Remote: deferred bind for %s -> %s now queued\n",
                 r != nullptr ? r->name : "remote", pendingBind.bulb.name);
}

void onRemotePrivilegeCommand(const esp_zb_zcl_privilege_command_message_t *message) {
  if (message == nullptr || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return;

  const esp_zb_zcl_cmd_info_t &info = message->info;
  if (info.src_address.addr_type != ESP_ZB_ZCL_ADDR_TYPE_SHORT) return;
  if (info.src_address.u.short_addr == 0x0000) return;  // Self-addressed loop.

  Remote *r = findByShort(info.src_address.u.short_addr);
  if (r == nullptr) {
    // First sight: reserve a slot; the IEEE is resolved by remotesTick.
    r = freeSlot();
    if (r == nullptr) {
      debugLogPrintf("Remote: no free slot for 0x%04x (press ignored)\n",
                    info.src_address.u.short_addr);
      return;
    }
    *r = Remote();
    r->used = true;
    r->shortAddr = info.src_address.u.short_addr;
    r->endpoint = info.src_endpoint;
    snprintf(r->name, sizeof(r->name), "Remote %u",
             (unsigned)(r - remotes + 1));
    debugLogPrintf("Remote: new steering device 0x%04x, resolving IEEE...\n",
                  r->shortAddr);
  }
  r->shortAddr = info.src_address.u.short_addr;
  r->endpoint = info.src_endpoint;
  r->lastSeenMs = millis();

  const RemoteEvent ev =
      normalizeEvent(info.cluster, info.command.id, (const uint8_t *)message->data,
                     message->size);
  if (ev == EV_COUNT) {
    debugLogPrintf("Remote: unknown command 0x%04x/0x%02x from 0x%04x\n",
                  info.cluster, info.command.id, info.src_address.u.short_addr);
    return;
  }
  strlcpy(r->lastEvent, eventName(ev), sizeof(r->lastEvent));
  debugLogPrintf("Remote: %s -> %s\n", r->name, eventName(ev));

  if (ev == EV_SCENE) {
    recallSceneById((const uint8_t *)message->data, message->size);
  } else {
    executeAction(actionMap[ev], *r);
  }

  // One webhook per distinct press (held buttons repeat the same command).
  if (r->lastHookEvent[0] == '\0' || strcmp(r->lastHookEvent, r->lastEvent) != 0 ||
      millis() - r->lastHookMs > 700) {
    strlcpy(r->lastHookEvent, r->lastEvent, sizeof(r->lastHookEvent));
    r->lastHookMs = millis();
    webHookEvent("remote_pressed", ieeeHexOf(r->ieee).c_str(), r->name,
                 eventName(ev));
  }
}

void onRemoteCustomCommand(const esp_zb_zcl_custom_cluster_command_message_t *message) {
  if (message == nullptr || message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) return;
  // Catch-all diagnostics: anything not covered by the privilege set lands
  // here, which makes unknown remotes visible in the serial log.
  debugLogPrintf("Remote: custom command cluster 0x%04x cmd 0x%02x from 0x%04x\n",
                message->info.cluster, message->info.command.id,
                message->info.src_address.u.short_addr);
}

}  // namespace

void remoteAttach(ZigbeeEP &ep) {
  ep.onPrivilegeCommand(onRemotePrivilegeCommand);
  ep.onCustomClusterCommand(onRemoteCustomCommand);

  // Standard commands IKEA steering devices send to the bound endpoint.
  ep.addPrivilegeCommand(0x0006, 0x00);  // On/Off: off
  ep.addPrivilegeCommand(0x0006, 0x01);  // On/Off: on
  ep.addPrivilegeCommand(0x0006, 0x02);  // On/Off: toggle
  ep.addPrivilegeCommand(0x0008, 0x01);  // Level: move
  ep.addPrivilegeCommand(0x0008, 0x02);  // Level: step
  ep.addPrivilegeCommand(0x0008, 0x03);  // Level: stop
  ep.addPrivilegeCommand(0x0008, 0x04);  // Level: step with on/off
  ep.addPrivilegeCommand(0x0008, 0x05);  // Level: move with on/off
  ep.addPrivilegeCommand(0x0008, 0x06);  // Level: stop with on/off
  ep.addPrivilegeCommand(0x0300, 0x01);  // Color: move hue
  ep.addPrivilegeCommand(0x0300, 0x02);  // Color: step hue
  ep.addPrivilegeCommand(0x0300, 0x04);  // Color: move saturation
  ep.addPrivilegeCommand(0x0300, 0x05);  // Color: step saturation
  ep.addPrivilegeCommand(0x0300, 0x4B);  // Color: move color temperature
  ep.addPrivilegeCommand(0x0300, 0x4C);  // Color: step color temperature
  ep.addPrivilegeCommand(0x0300, 0x4D);  // Color: step color temperature w/ on/off
  ep.addPrivilegeCommand(0x0005, 0x05);  // Scenes: recall
}

void remotesBegin() {
  prefs.begin(PREFS_NS, false);
  const uint8_t count = prefs.getUChar("count", 0);
  for (uint8_t i = 0; i < MAX_REMOTES && i < count; ++i) {
    String key = "r" + String(i);
    Remote &r = remotes[i];
    prefs.getBytes((key + "i").c_str(), r.ieee, sizeof(esp_zb_ieee_addr_t));
    const bool hasIeee = r.ieee[0] != 0 || r.ieee[7] != 0;
    if (!hasIeee) continue;
    String name = prefs.getString((key + "n").c_str(), "");
    if (name.length() == 0) name = "Remote " + String(i + 1);
    name.toCharArray(r.name, sizeof(r.name));
    r.used = true;
    r.shortAddr = 0xFFFF;  // Refreshed on the first press.
  }
  String csv = prefs.getString("m", "");
  if (csv.length() > 0) {
    int start = 0;
    for (uint8_t ev = 0; ev < EV_COUNT; ++ev) {
      const int comma = csv.indexOf(',', start);
      const String token =
          comma < 0 ? csv.substring(start) : csv.substring(start, comma);
      start = comma < 0 ? (int)csv.length() : comma + 1;
      const long value = token.toInt();
      if (value >= 0 && value < ACT_COUNT) actionMap[ev] = (uint8_t)value;
    }
  }
  debugLogPrintf("Remotes: %u registered\n", (unsigned)count);
}

void remotesTick() {
  if (!Zigbee.started() || !Zigbee.connected()) return;
  flushPendingBind();
  if (millis() - lastResolveMs < 2000) return;
  lastResolveMs = millis();
  resolveNextRemote();
}

bool remoteEnroll(const esp_zb_ieee_addr_t ieee, uint16_t shortAddr,
                  uint8_t endpoint) {
  Remote *r = findByIeee(ieee);
  if (r == nullptr) {
    r = freeSlot();
    if (r == nullptr) {
      debugLogPrintln("Remote: registry full, steering device ignored");
      return false;
    }
    *r = Remote();
    r->used = true;
    memcpy(r->ieee, ieee, sizeof(esp_zb_ieee_addr_t));
    snprintf(r->name, sizeof(r->name), "Remote %u",
             (unsigned)(r - remotes + 1));
  }
  r->shortAddr = shortAddr;
  r->endpoint = endpoint;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (&remotes[i] == r) {
      persistRemote(remotes[i], (int)i);
      persistCount();
      break;
    }
  }
  debugLogPrintf("Remote: enrolled %s (0x%04x ep %u)\n", r->name, shortAddr,
                endpoint);
  return true;
}

RemoteBindResult remoteBindToLight(const String &id, const String &bulbId) {
  Remote *r = findByApiId(id);
  if (r == nullptr) return REMOTE_BIND_UNKNOWN;
  Bulb *b = nullptr;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *cand = registryGet(i);
    if (cand != nullptr && bulbIeeeHex(cand).equalsIgnoreCase(bulbId)) {
      b = cand;
      break;
    }
  }
  if (b == nullptr) return REMOTE_BIND_UNKNOWN;

  // The remote's short address changes at every rejoin: prefer the fresh
  // value from the live network snapshots (matched by IEEE).
  uint16_t shortAddr = r->shortAddr;
  uint8_t endpoint = r->endpoint != 0 ? r->endpoint : 1;
  DeviceInfo net[14];
  size_t n = zigbeeMemberSnapshot(net, 7);
  n += zigbeeBoundSnapshot(net + n, 14 - n);
  for (size_t i = 0; i < n; ++i) {
    if (net[i].ieee[0] == 0 && net[i].ieee[7] == 0) continue;
    if (memcmp(net[i].ieee, r->ieee, sizeof(esp_zb_ieee_addr_t)) == 0 &&
        net[i].shortAddr != 0xFFFF && net[i].shortAddr != 0) {
      shortAddr = net[i].shortAddr;
      break;
    }
  }
  if (shortAddr == 0xFFFF || shortAddr == 0) {
    // The remote is asleep and unknown to the network: arm the bind so it
    // fires automatically on the first press (which is also what wakes the
    // remote and lets it accept the ZDO Bind requests).
    return armBindOnFirstPress(r->ieee, *b, endpoint) ? REMOTE_BIND_OK
                                                      : REMOTE_BIND_UNKNOWN;
  }
  if (zigbeeRemoteBindBusy()) return REMOTE_BIND_BUSY;
  if (!zigbeeQueueRemoteBind(r->ieee, shortAddr, endpoint, b))
    return REMOTE_BIND_UNKNOWN;
  r->shortAddr = shortAddr;
  r->endpoint = endpoint;
  return REMOTE_BIND_OK;
}

String remoteListJson() {
  String j = "[";
  bool first = true;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    const Remote &r = remotes[i];
    if (!r.used) continue;
    if (!first) j += ",";
    first = false;
    j += "{\"id\":\"";
    j += ieeeHexOf(r.ieee);
    j += "\",\"name\":\"";
    j += r.name;
    j += "\",\"short\":\"0x";
    j += String(r.shortAddr, HEX);
    j += "\",\"last_event\":\"";
    j += r.lastEvent;
    j += "\",\"last_seen_s\":";
    j += r.lastSeenMs == 0 ? -1 : (long)((millis() - r.lastSeenMs) / 1000);
    j += "}";
  }
  j += "]";
  return j;
}

size_t remoteCount() {
  uint8_t n = 0;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (remotes[i].used) ++n;
  }
  return n;
}

String remoteIdAt(size_t index) {
  uint8_t seen = 0;
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (!remotes[i].used) continue;
    if (seen == index) return ieeeHexOf(remotes[i].ieee);
    ++seen;
  }
  return String();
}

bool remoteRename(const String &id, const char *newName) {
  Remote *r = findByApiId(id);
  if (r == nullptr || newName == nullptr) return false;
  char sanitized[24];
  strlcpy(sanitized, newName, sizeof(sanitized));
  for (char *p = sanitized; *p != '\0'; ++p) {
    if (*p == '"' || *p == '\\' || (unsigned char)*p < 0x20 ||
        (unsigned char)*p > 0x7e) {
      *p = ' ';
    }
  }
  strlcpy(r->name, sanitized, sizeof(r->name));
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (&remotes[i] == r) {
      persistRemote(remotes[i], (int)i);
      break;
    }
  }
  return true;
}

bool remoteRemove(const String &id) {
  for (size_t i = 0; i < MAX_REMOTES; ++i) {
    if (!remotes[i].used) continue;
    if (!ieeeHexOf(remotes[i].ieee).equalsIgnoreCase(id)) continue;
    String key = "r" + String(i);
    prefs.remove((key + "i").c_str());
    prefs.remove((key + "n").c_str());
    remotes[i] = Remote();
    persistCount();
    debugLogPrintf("Remote: forgot %s\n", id.c_str());
    return true;
  }
  return false;
}

String remoteActionsJson() {
  String j = "{";
  for (uint8_t ev = 0; ev < EV_SCENE; ++ev) {  // "scene" is payload-driven.
    if (ev > 0) j += ",";
    j += "\"";
    j += eventName(ev);
    j += "\":\"";
    j += actionName(actionMap[ev]);
    j += "\"";
  }
  j += "}";
  return j;
}

bool remoteSetActions(const String &body, String &applied) {
  uint8_t wanted[EV_COUNT];
  memcpy(wanted, actionMap, sizeof(actionMap));
  bool any = false;
  for (uint8_t ev = 0; ev < EV_SCENE; ++ev) {
    String value;
    if (!jsonGetString(body, eventName(ev), value)) continue;
    uint8_t act = ACT_NONE;
    if (!actionFromName(value.c_str(), act)) return false;  // Invalid pair.
    wanted[ev] = act;
    any = true;
  }
  if (!any) return false;
  memcpy(actionMap, wanted, sizeof(actionMap));
  persistMap();
  applied = remoteActionsJson();
  return true;
}
