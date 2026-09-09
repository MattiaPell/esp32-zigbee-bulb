#include "zigbee_bulbs.h"

#include <Zigbee.h>

#include "esp_coexist.h"

#include "bulb_registry.h"
#include "config.h"
#include "remote_controls.h"
#include "status_led.h"
#include "web_hooks.h"
#include "zigbee_groups.h"

namespace {

// One controller endpoint bound to every bulb on the network.
ZigbeeColorDimmerSwitch bulbEP(BULB_ENDPOINT);

// --- Sync / resolution state ------------------------------------------------

uint32_t lastSyncMs = 0;
bool pairingOpenAtBootDone = false;
uint32_t pairingOpenedAtMs = 0;
uint8_t pairingSeconds = PAIRING_SECONDS;
// ZDO NWK-address resolution (IEEE -> short), one bulb at a time.
size_t resolveIndex = 0;
uint32_t lastResolveMs = 0;

// Unknown short sources seen in reports (short -> IEEE resolution queue).
constexpr size_t MAX_PENDING_SOURCES = 4;
uint16_t pendingSources[MAX_PENDING_SOURCES];
size_t pendingSourceCount = 0;
uint32_t lastSourceResolveMs = 0;

// --- New-device verification (bulb vs remote) --------------------------------
//
// Anything bound to our endpoint that is not yet in the registry must be
// verified before it can enter it: lamps expose on/off as an INPUT (server)
// cluster, remote controls only as OUTPUT (client). Verification path:
// resolve the short address (if missing) then read the ZDO simple
// descriptor of the device endpoint. Non-lights land on a RAM blacklist so
// joining remotes/steering devices never appear as "Bulb N".

constexpr size_t VERIFY_QUEUE = 4;
constexpr size_t VERIFY_BLACKLIST = 8;
constexpr uint8_t VERIFY_MAX_ATTEMPTS = 3;

struct VerifyJob {
  bool used = false;
  bool waitingShort = false;
  esp_zb_ieee_addr_t ieee = {0};
  uint16_t shortAddr = 0xFFFF;
  uint8_t endpoint = 0;
  uint8_t attempts = 0;
};

VerifyJob verifyQueue[VERIFY_QUEUE];
esp_zb_ieee_addr_t verifyBlacklist[VERIFY_BLACKLIST];
size_t verifyBlacklistCount = 0;
uint32_t lastVerifyMs = 0;

bool ieeeListContains(const esp_zb_ieee_addr_t list[], size_t count, const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < count; ++i) {
    if (memcmp(list[i], ieee, sizeof(esp_zb_ieee_addr_t)) == 0) return true;
  }
  return false;
}

// Registry of IEEE addresses assumed settled (verified lights).
bool ieeeIsKnownLight(const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < registryCount(); ++i) {
    if (memcmp(registryGet(i)->ieee, ieee, sizeof(esp_zb_ieee_addr_t)) == 0) return true;
  }
  return false;
}

void verifyEnqueue(const esp_zb_ieee_addr_t ieee, uint16_t shortAddr, uint8_t endpoint) {
  if (ieeeListContains(verifyBlacklist, verifyBlacklistCount, ieee)) {
    return;  // Already judged not-a-light.
  }
  if (ieeeIsKnownLight(ieee)) return;
  for (size_t i = 0; i < VERIFY_QUEUE; ++i) {
    if (verifyQueue[i].used &&
        memcmp(verifyQueue[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
      if (verifyQueue[i].shortAddr == 0xFFFF && shortAddr != 0xFFFF) {
        verifyQueue[i].shortAddr = shortAddr;
      }
      return;
    }
  }
  for (size_t i = 0; i < VERIFY_QUEUE; ++i) {
    if (!verifyQueue[i].used) {
      verifyQueue[i] = VerifyJob();
      verifyQueue[i].used = true;
      memcpy(verifyQueue[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t));
      verifyQueue[i].shortAddr = shortAddr;
      verifyQueue[i].endpoint = endpoint;
      Serial.printf("Zigbee: verifying bound device %u ep %u...\n",
                    endpoint, i);
      return;
    }
  }
}

void verifyJobDone(size_t index) {
  verifyQueue[index].used = false;
}
void verifyTick(uint32_t now) {
  if (now - lastVerifyMs < 1500) return;
  lastVerifyMs = now;
  for (size_t i = 0; i < VERIFY_QUEUE; ++i) {
    VerifyJob &job = verifyQueue[i];
    if (!job.used) continue;

    if (job.shortAddr == 0xFFFF) {
      if (job.waitingShort) {
        // Previous resolve is still pending; give it another tick.
        if (++job.attempts >= VERIFY_MAX_ATTEMPTS + 2) {
          Serial.println("Zigbee: verify gave up (no short address)");
          job.used = false;
        }
        return;
      }
      job.waitingShort = true;
      esp_zb_zdo_nwk_addr_req_param_t req = {};
      req.dst_nwk_addr = 0xFFFC;
      memcpy(req.ieee_addr_of_interest, job.ieee, sizeof(esp_zb_ieee_addr_t));
      req.request_type = 0;
      req.start_index = 0;
      if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
      esp_zb_zdo_nwk_addr_req(&req, [](esp_zb_zdp_status_t status,
                                       esp_zb_zdo_nwk_addr_rsp_t *resp, void *) {
        if (status != ESP_ZB_ZDP_STATUS_SUCCESS || resp == nullptr) return;
        for (size_t j = 0; j < VERIFY_QUEUE; ++j) {
          VerifyJob &w = verifyQueue[j];
          if (w.used && w.waitingShort &&
              memcmp(w.ieee, resp->ieee_addr, sizeof(esp_zb_ieee_addr_t)) == 0) {
            w.shortAddr = resp->nwk_addr;
            w.waitingShort = false;
          }
        }
      }, nullptr);
      esp_zb_lock_release();
      return;
    }

    esp_zb_zdo_simple_desc_req_param_t req = {};
    req.addr_of_interest = job.shortAddr;
    req.endpoint = job.endpoint;
    if (++job.attempts > VERIFY_MAX_ATTEMPTS) {
      // Unreachable (likely a sleeping battery device): give up for now,
      // the next binding sync will re-queue it.
      job.used = false;
      Serial.println("Zigbee: verify gave up (no descriptor response)");
      return;
    }
    if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
    esp_zb_zdo_simple_desc_req(&req, [](esp_zb_zdp_status_t status,
                                        esp_zb_af_simple_desc_1_1_t *desc, void *) {
      // Find the job that was waiting for this answer (single outstanding
      // request at any time by construction).
      VerifyJob *job = nullptr;
      for (size_t j = 0; j < VERIFY_QUEUE; ++j) {
        if (verifyQueue[j].used && !verifyQueue[j].waitingShort) {
          job = &verifyQueue[j];
          break;
        }
      }
      if (job == nullptr) return;

      bool isLight = false;
      bool isRemote = false;
      if (status == ESP_ZB_ZDP_STATUS_SUCCESS && desc != nullptr) {
        for (uint8_t c = 0; c < desc->app_input_cluster_count; ++c) {
          if (desc->app_cluster_list[c] == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            isLight = true;
            break;
          }
        }
        if (!isLight) {
          // Steering devices expose on/off only as OUTPUT (client) clusters.
          for (uint8_t c = desc->app_input_cluster_count;
               c < desc->app_input_cluster_count + desc->app_output_cluster_count;
               ++c) {
            if (desc->app_cluster_list[c] == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
              isRemote = true;
              break;
            }
          }
        }
        if (!isLight && !isRemote) {
          Serial.printf("Zigbee: bound device 0x%04x ep %u is not a light (device_id 0x%04x, no on/off server)\n",
                        job->shortAddr, job->endpoint, desc->app_device_id);
        }
      }
      job->used = false;
      if (isLight) {
        Bulb *added = registryAdd(job->ieee);
        if (added != nullptr) {
          if (job->shortAddr != 0xFFFF) added->shortAddr = job->shortAddr;
          if (job->endpoint != 0) added->endpoint = job->endpoint;
          Serial.println("Zigbee: verified bound device is a light: registered.");
        }
      } else if (isRemote) {
        remoteEnroll(job->ieee, job->shortAddr, job->endpoint);
      } else {
        if (verifyBlacklistCount < VERIFY_BLACKLIST) {
          memcpy(verifyBlacklist[verifyBlacklistCount++], job->ieee, sizeof(esp_zb_ieee_addr_t));
        }
      }
    }, nullptr);
    esp_zb_lock_release();
    return;
  }
}

// Boot state resend queue: one bulb per pass.
bool resendPending = false;
size_t resendIndex = 0;
uint32_t lastResendMs = 0;

// Readback rotation: continuous staggered attribute reads (see tick).
constexpr uint32_t READBACK_INTERVAL_MS = 8000;
size_t readbackIndex = 0;
uint32_t lastReadbackMs = 0;

// --- Network members snapshot ------------------------------------------------
//
// Direct iteration over the Zigbee NWK neighbor table (synchronous, works
// for sleepy children too, and exposes the relationship so the remote shows
// up as our child, not just active routers).

constexpr size_t MEMBER_CAP = 12;
constexpr uint32_t MEMBER_REFRESH_MS = 20000;
DeviceInfo members[MEMBER_CAP];
size_t memberCount = 0;
uint32_t lastMembersMs = 0;

uint16_t kelvinToMireds(int kelvin) {
  if (kelvin < MIN_KELVIN) kelvin = MIN_KELVIN;
  if (kelvin > MAX_KELVIN) kelvin = MAX_KELVIN;
  return (uint16_t)(1000000UL / (uint32_t)kelvin);
}

uint16_t clampTransition(uint16_t transitionDs) {
  if (transitionDs > MAX_TRANSITION_DS) return MAX_TRANSITION_DS;
  return transitionDs;
}

Bulb *bulbByIeeeFromResponse(const esp_zb_ieee_addr_t ieee) {
  return registryFindByIeee(ieee);
}

// Collective group path: usable when every reachable bulb is a confirmed
// member of the shared group (USE_ZIGBEE_GROUPS false forces unicast).
bool groupUsable() {
  if (!USE_ZIGBEE_GROUPS || registryCount() == 0) return false;
  for (size_t i = 0; i < registryCount(); ++i) {
    const Bulb *b = registryGet(i);
    if (bulbReady(b) && !b->groupMember) return false;
  }
  return true;
}

size_t readyBulbCount() {
  size_t n = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    if (bulbReady(registryGet(i))) ++n;
  }
  return n;
}

// Re-arms the boot state resend (idempotent): used when a bulb becomes
// addressable late, after the first pass already skipped it.
void armBootResend() {
  if (!resendPending) {
    resendPending = true;
    resendIndex = 0;
  }
}

void noteShortSource(uint16_t shortAddr) {
  for (size_t i = 0; i < registryCount(); ++i) {
    if (registryGet(i)->shortAddr == shortAddr) return;  // Already known.
  }
  for (size_t i = 0; i < pendingSourceCount; ++i) {
    if (pendingSources[i] == shortAddr) return;
  }
  if (pendingSourceCount < MAX_PENDING_SOURCES) {
    pendingSources[pendingSourceCount++] = shortAddr;
  }
}

// --- Report callbacks (ZCL attribute updates from the bulbs) ----------------

// Per-bulb diagnostics: every report marks the bulb as seen; the link
// metrics come from the NWK neighbor table (see zigbeeRequestMembers).
void noteReport(Bulb *b) {
  if (b == nullptr) return;
  if (!b->online) b->online = true;
  b->lastSeenMs = millis();
}

// Command bookkeeping: the default-response callback attributes failures to
// the most recently commanded bulb (commands are staggered, one bulb at a
// time, so the attribution is unambiguous in practice).
Bulb *lastCommanded = nullptr;

void noteCommandSent(Bulb *b) {
  if (b == nullptr) return;
  lastCommanded = b;
  if (b->cmdSent < 0xFFFF) ++b->cmdSent;
}

void onBulbDefaultResponse(zb_cmd_type_t respToCmd, esp_zb_zcl_status_t status) {
  if (status == ESP_ZB_ZCL_STATUS_SUCCESS) return;
  Bulb *b = lastCommanded;
  if (b == nullptr) return;
  if (b->cmdFailed < 0xFFFF) ++b->cmdFailed;
  b->lastFailStatus = (uint8_t)status;
  Serial.printf("Zigbee: %s command %u failed (status %d: %s)\n", b->name,
                (unsigned)respToCmd, (int)status,
                esp_zb_zcl_status_to_name(status));
}

Bulb *bulbFromSource(uint8_t srcEndpoint, const esp_zb_zcl_addr_t &source) {
  if (source.addr_type == ESP_ZB_ZCL_ADDR_TYPE_IEEE) {
    return registryFindByIeee(source.u.ieee_addr);
  }
  if (source.addr_type != ESP_ZB_ZCL_ADDR_TYPE_SHORT) return nullptr;

  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (b->shortAddr == source.u.short_addr && b->endpoint == srcEndpoint) {
      return b;
    }
  }
  // Unknown short address: queue an IEEE lookup so future reports match.
  noteShortSource(source.u.short_addr);
  return nullptr;
}

void onBulbStateReport(bool on, uint8_t srcEndpoint, esp_zb_zcl_addr_t source) {
  Bulb *b = bulbFromSource(srcEndpoint, source);
  if (b == nullptr) return;
  noteReport(b);
  if (b->state.power != on) {
    b->state.power = on;
    registryMarkDirty();
  }
}

void onBulbLevelReport(uint8_t level, uint8_t srcEndpoint, esp_zb_zcl_addr_t source) {
  Bulb *b = bulbFromSource(srcEndpoint, source);
  if (b == nullptr) return;
  noteReport(b);
  if (b->state.level != level) {
    b->state.level = level;
    registryMarkDirty();
  }
}

void onBulbColorReport(uint8_t red, uint8_t green, uint8_t blue,
                       uint8_t srcEndpoint, esp_zb_zcl_addr_t source) {
  Bulb *b = bulbFromSource(srcEndpoint, source);
  if (b == nullptr) return;
  noteReport(b);
  // Store the color but do not switch the mode: a warm-white bulb reports
  // its own XY point, which is not an RGB command.
  if (b->state.red != red || b->state.green != green || b->state.blue != blue) {
    b->state.red = red;
    b->state.green = green;
    b->state.blue = blue;
    registryMarkDirty();
  }
}

// --- Raw ZCL commands (per-bulb unicast) ------------------------------------
//
// The library class covers on/off with per-device overloads; level, color
// temperature and color XY need transitions, so they are sent directly.
// Addressing: short address + endpoint present (16-bit unicast).

bool sendRawCommand(void (*request)(void *), void *cmd) {
  if (!esp_zb_lock_acquire(portMAX_DELAY)) return false;
  request(cmd);
  esp_zb_lock_release();
  return true;
}

void sendMoveToLevelWithOnOff(Bulb *bulb, uint8_t level, uint16_t transitionDs) {
  esp_zb_zcl_move_to_level_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_endpoint = bulb->endpoint;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = bulb->shortAddr;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  cmd.level = level;
  cmd.transition_time = transitionDs;
  sendRawCommand([](void *p) {
    esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(
        (esp_zb_zcl_move_to_level_cmd_t *)p);
  }, &cmd);
}

void sendMoveToColorTemperature(Bulb *bulb, uint16_t mireds, uint16_t transitionDs) {
  esp_zb_zcl_color_move_to_color_temperature_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_endpoint = bulb->endpoint;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = bulb->shortAddr;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  cmd.color_temperature = mireds;
  cmd.transition_time = transitionDs;
  sendRawCommand([](void *p) {
    esp_zb_zcl_color_move_to_color_temperature_cmd_req(
        (esp_zb_zcl_color_move_to_color_temperature_cmd_t *)p);
  }, &cmd);
}

void sendMoveToColor(Bulb *bulb, uint16_t x, uint16_t y, uint16_t transitionDs) {
  esp_zb_zcl_color_move_to_color_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_endpoint = bulb->endpoint;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = bulb->shortAddr;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  cmd.color_x = x;
  cmd.color_y = y;
  cmd.transition_time = transitionDs;
  sendRawCommand([](void *p) {
    esp_zb_zcl_color_move_to_color_cmd_req((esp_zb_zcl_color_move_to_color_cmd_t *)p);
  }, &cmd);
}

// --- Binding table reconciliation -------------------------------------------

void syncRegistryWithBindings() {
  std::list<zb_device_params_t *> bound = bulbEP.getBoundDevices();

  bool seen[MAX_BULBS] = {false};
  // The first pass after boot mirrors "what was already there": the boot
  // event covers it, so online/offline notifications start from the next
  // pass only.
  static bool firstSyncDone = false;
  const bool notify = firstSyncDone;

  for (zb_device_params_t *device : bound) {
    Bulb *b = nullptr;
    if (device->ieee_addr[0] != 0 || device->ieee_addr[7] != 0) {
      b = registryFindByIeee(device->ieee_addr);
      if (b == nullptr) {
        // Not registered yet: verify it is actually a light before adding.
        verifyEnqueue(device->ieee_addr, device->short_addr, device->endpoint);
        continue;
      }
    } else {
      // Bound by short address only: match a known bulb by short address.
      for (size_t i = 0; i < registryCount(); ++i) {
        if (registryGet(i)->shortAddr == device->short_addr) {
          b = registryGet(i);
          break;
        }
      }
    }
    if (b == nullptr) continue;

    if (device->short_addr != 0xFFFF) b->shortAddr = device->short_addr;
    if (device->endpoint != 0) b->endpoint = device->endpoint;
    if (!b->online) {
      b->online = true;
      if (notify) {
        webHookEvent("bulb_online", bulbIeeeHex(b).c_str(), b->name);
      }
    }
    if (!b->groupMember) {
      groupEnroll(b);  // Freshly reachable: queue group membership.
    }
    size_t index = b - registryGet(0);
    if (index < MAX_BULBS) seen[index] = true;
  }

  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (!seen[i] && b->online) {
      b->online = false;
      if (notify) {
        webHookEvent("bulb_offline", bulbIeeeHex(b).c_str(), b->name);
      }
      Serial.printf("Zigbee: %s not in binding table (offline)\n", b->name);
    }
  }
  firstSyncDone = true;
}

// --- ZDO address resolution ---------------------------------------------------
//
// ZDO requests enqueue into the ZBOSS scheduler and must run while holding
// the Zigbee lock, otherwise they race with the stack task (observed as a
// vPortExitCritical assert).

void resolveNextBulbShortAddress() {
  if (registryCount() == 0) return;
  for (size_t attempt = 0; attempt < registryCount(); ++attempt) {
    Bulb *b = registryGet(resolveIndex);
    resolveIndex = (resolveIndex + 1) % registryCount();
    if (b != nullptr && b->shortAddr == 0xFFFF) {
      esp_zb_zdo_nwk_addr_req_param_t req = {};  // Copied synchronously.
      // Broadcast to all routers: the coordinator's own address map lookup
      // does not return joined routers, but the device answers for itself.
      req.dst_nwk_addr = 0xFFFC;
      memcpy(req.ieee_addr_of_interest, b->ieee, sizeof(esp_zb_ieee_addr_t));
      req.request_type = 0;  // Single device response.
      req.start_index = 0;
      if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
      Serial.printf("Zigbee: resolving short address for %s...\n", b->name);
      esp_zb_zdo_nwk_addr_req(&req, [](esp_zb_zdp_status_t status,
                                       esp_zb_zdo_nwk_addr_rsp_t *resp, void *) {
        if (status != ESP_ZB_ZDP_STATUS_SUCCESS || resp == nullptr) {
          Serial.printf("Zigbee: short address resolve failed (status %d)\n",
                        (int)status);
          return;
        }
        Bulb *found = bulbByIeeeFromResponse(resp->ieee_addr);
        if (found != nullptr && resp->nwk_addr != 0xFFFF &&
            found->shortAddr != resp->nwk_addr) {
          found->shortAddr = resp->nwk_addr;
          Serial.printf("Zigbee: %s short address 0x%04x\n", found->name,
                        resp->nwk_addr);
          armBootResend();
        }
      }, nullptr);
      esp_zb_lock_release();
      return;
    }
  }
}

void resolveNextUnknownSource() {
  if (pendingSourceCount == 0) return;
  const uint16_t shortAddr = pendingSources[0];
  for (size_t i = 0; i + 1 < pendingSourceCount; ++i) {
    pendingSources[i] = pendingSources[i + 1];
  }
  --pendingSourceCount;

  esp_zb_zdo_ieee_addr_req_param_t req = {};  // Copied synchronously.
  req.dst_nwk_addr = 0xFFFC;  // Broadcast: the device answers for itself.
  req.addr_of_interest = shortAddr;
  req.request_type = 0;
  req.start_index = 0;
  if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
  Serial.printf("Zigbee: resolving IEEE for source 0x%04x...\n", shortAddr);
  esp_zb_zdo_ieee_addr_req(&req, [](esp_zb_zdp_status_t status,
                                    esp_zb_zdo_ieee_addr_rsp_t *resp, void *) {
    if (status != ESP_ZB_ZDP_STATUS_SUCCESS || resp == nullptr) {
      Serial.printf("Zigbee: IEEE resolve failed (status %d)\n", (int)status);
      return;
    }
    Bulb *found = bulbByIeeeFromResponse(resp->ieee_addr);
    if (found != nullptr) {
      if (found->shortAddr != resp->nwk_addr) {
        found->shortAddr = resp->nwk_addr;
        Serial.printf("Zigbee: %s remapped to 0x%04x\n", found->name,
                      resp->nwk_addr);
        armBootResend();
      }
    }
  }, nullptr);
  esp_zb_lock_release();
}

// --- Unbind (device removal) --------------------------------------------------

void unbindResponseStub(esp_zb_zdp_status_t, void *) {
  // Unbind responses are fire-and-forget; the follow-up binding-table sync
  // confirms the removal.
}

void sendUnbindForCluster(const Bulb *bulb, uint16_t clusterId) {
  esp_zb_zdo_bind_req_param_t req = {};  // Copied synchronously.
  esp_zb_get_long_address(req.src_address);
  req.src_endp = BULB_ENDPOINT;
  req.cluster_id = clusterId;
  req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
  memcpy(req.dst_address_u.addr_long, bulb->ieee, sizeof(esp_zb_ieee_addr_t));
  req.dst_endp = bulb->endpoint != 0 ? bulb->endpoint : 1;  // Common bulb EP.
  req.req_dst_addr = esp_zb_get_short_address();
  if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
  esp_zb_zdo_device_unbind_req(&req, unbindResponseStub, nullptr);
  esp_zb_lock_release();
}

}  // namespace

// --- Public API ----------------------------------------------------------------

bool bulbReady(const Bulb *bulb) {
  return bulb != nullptr && bulb->online && bulb->shortAddr != 0xFFFF &&
         bulb->endpoint != 0;
}

void zigbeeBegin() {
  bulbEP.setManufacturerAndModel("MattiaPell", "esp32-zigbee-bulb");
  bulbEP.allowMultipleBinding(true);
  bulbEP.onLightStateChangeWithSource(onBulbStateReport);
  bulbEP.onLightLevelChangeWithSource(onBulbLevelReport);
  bulbEP.onLightColorChangeWithSource(onBulbColorReport);
  bulbEP.onDefaultResponse(onBulbDefaultResponse);
  Zigbee.addEndpoint(&bulbEP);

  esp_coex_wifi_i154_enable();  // Wi-Fi + 802.15.4 coexistence (ESP32-C6).

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start. Restarting...");
    statusLedSetMode(StatusLedMode::Error);
    const uint32_t errorStarted = millis();
    while (millis() - errorStarted < 2000) {
      statusLedTick();
      delay(10);
    }
    ESP.restart();
  }
  Serial.println("Zigbee coordinator started.");

  remoteAttach(bulbEP);  // Intercept remote/steering device commands.
}

void zigbeeOpenPairing(uint8_t seconds) {
  Zigbee.openNetwork(seconds);
  pairingOpenedAtMs = millis();
  pairingSeconds = seconds;
  Serial.printf("Pairing open for %u seconds.\n", seconds);
}

bool zigbeePairingActive() {
  return pairingOpenedAtMs != 0 &&
         millis() - pairingOpenedAtMs < (uint32_t)pairingSeconds * 1000;
}

size_t zigbeeBoundDeviceCount() {
  return bulbEP.getBoundDevices().size();
}

void zigbeeRemoveDevice(Bulb *bulb) {
  if (bulb == nullptr) return;
  sendUnbindForCluster(bulb, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF);
  sendUnbindForCluster(bulb, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL);
  sendUnbindForCluster(bulb, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL);

  zb_device_params_t probe = {};
  memcpy(probe.ieee_addr, bulb->ieee, sizeof(esp_zb_ieee_addr_t));
  probe.short_addr = bulb->shortAddr;
  probe.endpoint = bulb->endpoint;
  bulbEP.removeBoundDevice(&probe);

  groupForget(bulb);  // Tell the bulb to leave the group (fire and forget).
  registryRemove(bulb);
}

void zigbeeRefreshStates() {
  // The readback loop rotates continuously; a forced refresh just restarts it.
  readbackIndex = 0;
  lastReadbackMs = 0;
}

void zigbeeTick() {
  if (!Zigbee.started()) return;

  const uint32_t now = millis();

  if (!Zigbee.connected()) return;

  // Bindings + registry reconciliation.
  if (now - lastSyncMs >= 5000 || lastSyncMs == 0) {
    lastSyncMs = now;
    syncRegistryWithBindings();
  }

  // On the first sync that sees registered bulbs, arm the boot queues:
  // resend stored states, then read back the real ones.
  static bool bootQueuesArmed = false;
  if (!bootQueuesArmed && registryCount() > 0) {
    bootQueuesArmed = true;
    resendPending = true;
    resendIndex = 0;
    zigbeeRefreshStates();
  }

  // While no bulb is bound, keep a pairing window open so the first bulb can
  // join without using the UI. The first window opens a few seconds after
  // the network forms; expired windows are reopened immediately.
  if (registryCount() == 0 && !zigbeePairingActive()) {
    if (!pairingOpenAtBootDone) {
      if (now > 5000) {
        zigbeeOpenPairing(PAIRING_SECONDS);
        pairingOpenAtBootDone = true;
      }
    } else {
      zigbeeOpenPairing(PAIRING_SECONDS);
    }
  }

  // Resolve missing short addresses (IEEE -> short), one per pass.
  if (now - lastResolveMs >= 1000) {
    lastResolveMs = now;
    resolveNextBulbShortAddress();
  }

  // Resolve unknown report sources (short -> IEEE), one per pass.
  if (now - lastSourceResolveMs >= 2000) {
    lastSourceResolveMs = now;
    resolveNextUnknownSource();
  }

  // Verify newly-bound devices (bulb vs remote/steering device).
  verifyTick(now);

  // Group membership enrollment (staggered Add Group commands).
  groupTick(now);

  // Remote control housekeeping (IEEE resolution for new remotes).
  remotesTick();

  // Network member snapshot refresh.
  if (now - lastMembersMs >= MEMBER_REFRESH_MS) {
    lastMembersMs = now;
    zigbeeRequestMembers();
  }

  // Boot state resend: one bulb per pass.
  if (resendPending && now - lastResendMs >= 400) {
    lastResendMs = now;
    Bulb *b = registryGet(resendIndex);
    resendIndex++;
    if (b != nullptr && bulbReady(b)) {
      bulbSendFullState(b);
    }
    if (resendIndex >= registryCount()) {
      resendPending = false;
      Serial.println("Zigbee: stored states resent after boot.");
    }
  }

  // Continuous readback rotation: one bulb every READBACK_INTERVAL_MS, so
  // physical changes (IKEA remote, factory-reset bulbs that stopped
  // reporting) reach the registry and the UI within a few seconds.
  if (registryCount() > 0 && now - lastReadbackMs >= READBACK_INTERVAL_MS) {
    lastReadbackMs = now;
    for (size_t n = 0; n < registryCount(); ++n) {
      readbackIndex = (readbackIndex + 1) % registryCount();
      Bulb *b = registryGet(readbackIndex);
      if (b != nullptr && bulbReady(b)) {
        Serial.printf("Readback -> %s 0x%04x ep %u\n", b->name, b->shortAddr, b->endpoint);
        bulbEP.getLightState(b->endpoint, b->shortAddr);
        bulbEP.getLightLevel(b->endpoint, b->shortAddr);
        bulbEP.getLightColor(b->endpoint, b->shortAddr);
        break;
      }
    }
  }
}

void bulbSendOn(Bulb *bulb) {
  if (!bulbReady(bulb)) return;
  bulbEP.lightOn(bulb->endpoint, bulb->shortAddr);
  noteCommandSent(bulb);
  bulb->state.power = true;
  registryMarkDirty();
}

void bulbSendOff(Bulb *bulb) {
  if (!bulbReady(bulb)) return;
  bulbEP.lightOff(bulb->endpoint, bulb->shortAddr);
  noteCommandSent(bulb);
  bulb->state.power = false;
  registryMarkDirty();
}

void bulbSendToggle(Bulb *bulb) {
  if (!bulbReady(bulb)) return;
  bulbEP.lightToggle(bulb->endpoint, bulb->shortAddr);
  noteCommandSent(bulb);
  bulb->state.power = !bulb->state.power;
  registryMarkDirty();
}

void bulbSendBrightness(Bulb *bulb, uint8_t pct, uint16_t transitionDs) {
  if (!bulbReady(bulb)) return;
  if (pct == 0) {
    bulbSendOff(bulb);
    return;
  }
  uint8_t level = (uint8_t)((pct * 255 + 50) / 100);
  sendMoveToLevelWithOnOff(bulb, level, clampTransition(transitionDs));
  noteCommandSent(bulb);
  bulb->state.power = true;
  bulb->state.level = level;
  registryMarkDirty();
}

void bulbSendKelvin(Bulb *bulb, int kelvin, uint16_t transitionDs) {
  if (!bulbReady(bulb)) return;
  sendMoveToColorTemperature(bulb, kelvinToMireds(kelvin),
                             clampTransition(transitionDs));
  noteCommandSent(bulb);
  bulb->state.mode = BulbColorMode::White;
  bulb->state.kelvin = (uint16_t)constrain(kelvin, MIN_KELVIN, MAX_KELVIN);
  registryMarkDirty();
}

void bulbSendRgb(Bulb *bulb, uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs) {
  if (!bulbReady(bulb)) return;
  espXyColor_t xy = espRgbToXYColor(r, g, b);
  sendMoveToColor(bulb, xy.x, xy.y, clampTransition(transitionDs));
  noteCommandSent(bulb);
  bulb->state.mode = BulbColorMode::Rgb;
  bulb->state.red = r;
  bulb->state.green = g;
  bulb->state.blue = b;
  registryMarkDirty();
}

void zigbeeRequestMembers() {
  if (!Zigbee.started() || !Zigbee.connected()) return;
  size_t n = 0;
  esp_zb_nwk_info_iterator_t it = ESP_ZB_NWK_INFO_ITERATOR_INIT;
  esp_zb_nwk_neighbor_info_t info;
  while (n < MEMBER_CAP) {
    esp_err_t err = esp_zb_nwk_get_next_neighbor(&it, &info);
    if (err != ESP_OK) {
      if (n == 0) {
        Serial.printf("Neighbor table read: err=%d (table empty or not supported)\n", err);
      }
      break;
    }
    members[n].deviceType = info.device_type;
    members[n].shortAddr = info.short_addr;
    memcpy(members[n].ieee, info.ieee_addr, sizeof(esp_zb_ieee_addr_t));
    // Link diagnostics: attach to the bulb this neighbor belongs to.
    Bulb *bulb = registryFindByIeee(info.ieee_addr);
    if (bulb == nullptr) {
      for (size_t j = 0; j < registryCount(); ++j) {
        if (registryGet(j)->shortAddr == info.short_addr) {
          bulb = registryGet(j);
          break;
        }
      }
    }
    if (bulb != nullptr) {
      bulb->lqi = info.lqi;
      bulb->rssi = info.rssi;
    }
    ++n;
  }
  memberCount = n;
  Serial.printf("Network members refreshed: %u\n", (unsigned)n);
}

size_t zigbeeMemberSnapshot(DeviceInfo *out, size_t cap) {
  size_t n = memberCount < cap ? memberCount : cap;
  for (size_t i = 0; i < n; ++i) out[i] = members[i];
  return n;
}

size_t zigbeeBoundSnapshot(DeviceInfo *out, size_t cap) {
  std::list<zb_device_params_t *> bound = bulbEP.getBoundDevices();
  size_t n = 0;
  for (zb_device_params_t *d : bound) {
    if (n >= cap) break;
    memcpy(out[n].ieee, d->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    out[n].shortAddr = d->short_addr;
    out[n].deviceType = 255;  // "bound device" marker (type unknown here).
    ++n;
  }
  return n;
}

int bulbSendAllOn() {
  if (groupUsable() && groupSendOn()) {
    for (size_t i = 0; i < registryCount(); ++i) {
      Bulb *b = registryGet(i);
      if (bulbReady(b)) b->state.power = true;  // Each resumes its own state.
    }
    registryMarkDirty();
    Serial.println("All bulbs on (group frame).");
    return (int)readyBulbCount();
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (bulbReady(b)) {
      bulbSendOn(b);  // The bulb resumes its own last level and color.
      ++applied;
    }
  }
  return applied;
}

int bulbSendAllOff() {
  if (groupUsable() && groupSendOff()) {
    for (size_t i = 0; i < registryCount(); ++i) {
      registryGet(i)->state.power = false;  // Keep stored state honest.
    }
    registryFlush();
    Serial.println("Kill switch: all bulbs off (group frame).");
    return (int)readyBulbCount();
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (bulbReady(b)) {
      bulbSendOff(b);
      ++applied;
    } else {
      b->state.power = false;  // Keep the stored state honest even if offline.
    }
  }
  registryFlush();
  return applied;
}

int bulbSendAllBrightness(uint8_t pct, uint16_t transitionDs) {
  if (pct == 0) return bulbSendAllOff();
  if (groupUsable() && groupSendBrightness(pct, transitionDs)) {
    const uint8_t level = (uint8_t)((pct * 255 + 50) / 100);
    for (size_t i = 0; i < registryCount(); ++i) {
      Bulb *b = registryGet(i);
      if (bulbReady(b)) {
        b->state.power = true;
        b->state.level = level;
      }
    }
    registryMarkDirty();
    Serial.printf("All bulbs to %u %% (group frame).\n", pct);
    return (int)readyBulbCount();
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (bulbReady(b)) {
      bulbSendBrightness(b, pct, transitionDs);
      ++applied;
    }
  }
  return applied;
}

int bulbSendAllKelvin(int kelvin, uint16_t transitionDs) {
  if (groupUsable() && groupSendKelvin(kelvin, transitionDs)) {
    const uint16_t clamped = (uint16_t)constrain(kelvin, MIN_KELVIN, MAX_KELVIN);
    for (size_t i = 0; i < registryCount(); ++i) {
      Bulb *b = registryGet(i);
      if (bulbReady(b)) {
        b->state.mode = BulbColorMode::White;
        b->state.kelvin = clamped;
      }
    }
    registryMarkDirty();
    Serial.printf("All bulbs to %d K (group frame).\n", clamped);
    return (int)readyBulbCount();
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *b = registryGet(i);
    if (bulbReady(b)) {
      bulbSendKelvin(b, kelvin, transitionDs);
      ++applied;
    }
  }
  return applied;
}

int bulbSendAllRgb(uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs) {
  if (groupUsable() && groupSendRgb(r, g, b, transitionDs)) {
    for (size_t i = 0; i < registryCount(); ++i) {
      Bulb *bulb = registryGet(i);
      if (bulbReady(bulb)) {
        bulb->state.mode = BulbColorMode::Rgb;
        bulb->state.red = r;
        bulb->state.green = g;
        bulb->state.blue = b;
      }
    }
    registryMarkDirty();
    Serial.println("All bulbs to RGB (group frame).");
    return (int)readyBulbCount();
  }
  int applied = 0;
  for (size_t i = 0; i < registryCount(); ++i) {
    Bulb *bulb = registryGet(i);
    if (bulbReady(bulb)) {
      bulbSendRgb(bulb, r, g, b, transitionDs);
      ++applied;
    }
  }
  return applied;
}

void bulbSendFullState(Bulb *bulb) {
  if (bulb != nullptr) {
    bulbApplyState(bulb, bulb->state, DEFAULT_TRANSITION_DS);
  }
}

void bulbApplyState(Bulb *bulb, const BulbState &wanted, uint16_t transitionDs) {
  if (!bulbReady(bulb)) return;
  noteCommandSent(bulb);
  if (!wanted.power) {
    bulbEP.lightOff(bulb->endpoint, bulb->shortAddr);
    bulb->state.power = false;
    return;
  }
  // Move-to-level with the on/off flag: turns the bulb on and fades to the
  // wanted level even if it was off.
  sendMoveToLevelWithOnOff(bulb, wanted.level, transitionDs);
  if (wanted.mode == BulbColorMode::White) {
    sendMoveToColorTemperature(bulb, kelvinToMireds(wanted.kelvin), 0);
  } else {
    espXyColor_t xy = espRgbToXYColor(wanted.red, wanted.green, wanted.blue);
    sendMoveToColor(bulb, xy.x, xy.y, 0);
  }
  bulb->state = wanted;
}
