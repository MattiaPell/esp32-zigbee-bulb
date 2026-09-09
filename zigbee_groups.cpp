#include "zigbee_groups.h"

#include <Zigbee.h>

#include "config.h"
#include "debug_log.h"
#include "zigbee_bulbs.h"

namespace {

// --- Enrollment queue: one Add Group per pass, staggered --------------------

constexpr size_t ENROLL_QUEUE = MAX_BULBS;
constexpr uint8_t ENROLL_MAX_ATTEMPTS = 5;
constexpr uint32_t ENROLL_INTERVAL_MS = 600;

struct EnrollJob {
  bool used = false;
  esp_zb_ieee_addr_t ieee = {0};
  uint8_t attempts = 0;
};

EnrollJob enrollQueue[ENROLL_QUEUE];
uint32_t lastEnrollMs = 0;

bool sendGroupFrame(void (*request)(void *), void *cmd) {
  if (!esp_zb_lock_acquire(portMAX_DELAY)) return false;
  request(cmd);
  esp_zb_lock_release();
  return true;
}

void sendGroupOnOff(bool on) {
  esp_zb_zcl_on_off_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = ZIGBEE_GROUP_ID;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  cmd.on_off_cmd_id = on ? ESP_ZB_ZCL_CMD_ON_OFF_ON_ID : ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
  sendGroupFrame([](void *p) {
    esp_zb_zcl_on_off_cmd_req((esp_zb_zcl_on_off_cmd_t *)p);
  }, &cmd);
}

void sendGroupMoveToLevel(uint8_t level, uint16_t transitionDs) {
  esp_zb_zcl_move_to_level_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = ZIGBEE_GROUP_ID;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  cmd.level = level;
  cmd.transition_time = transitionDs;
  sendGroupFrame([](void *p) {
    esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(
        (esp_zb_zcl_move_to_level_cmd_t *)p);
  }, &cmd);
}

void sendGroupColorTemperature(uint16_t mireds, uint16_t transitionDs) {
  esp_zb_zcl_color_move_to_color_temperature_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = ZIGBEE_GROUP_ID;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  cmd.color_temperature = mireds;
  cmd.transition_time = transitionDs;
  sendGroupFrame([](void *p) {
    esp_zb_zcl_color_move_to_color_temperature_cmd_req(
        (esp_zb_zcl_color_move_to_color_temperature_cmd_t *)p);
  }, &cmd);
}

void sendGroupMoveToColor(uint16_t x, uint16_t y, uint16_t transitionDs) {
  esp_zb_zcl_color_move_to_color_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = ZIGBEE_GROUP_ID;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
  cmd.color_x = x;
  cmd.color_y = y;
  cmd.transition_time = transitionDs;
  sendGroupFrame([](void *p) {
    esp_zb_zcl_color_move_to_color_cmd_req((esp_zb_zcl_color_move_to_color_cmd_t *)p);
  }, &cmd);
}

uint16_t kelvinToMireds(int kelvin) {
  if (kelvin < MIN_KELVIN) kelvin = MIN_KELVIN;
  if (kelvin > MAX_KELVIN) kelvin = MAX_KELVIN;
  return (uint16_t)(1000000UL / (uint32_t)kelvin);
}

uint16_t clampTransition(uint16_t transitionDs) {
  if (transitionDs > MAX_TRANSITION_DS) return MAX_TRANSITION_DS;
  return transitionDs;
}

}  // namespace

bool groupEnroll(Bulb *bulb) {
  if (bulb == nullptr || bulb->groupMember) return true;
  for (size_t i = 0; i < ENROLL_QUEUE; ++i) {
    if (enrollQueue[i].used &&
        memcmp(enrollQueue[i].ieee, bulb->ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
      return true;  // Already queued.
    }
  }
  for (size_t i = 0; i < ENROLL_QUEUE; ++i) {
    if (!enrollQueue[i].used) {
      enrollQueue[i] = EnrollJob();
      memcpy(enrollQueue[i].ieee, bulb->ieee, sizeof(esp_zb_ieee_addr_t));
      return true;
    }
  }
  return false;
}

void groupTick(uint32_t now) {
  if (now - lastEnrollMs < ENROLL_INTERVAL_MS) return;
  lastEnrollMs = now;

  for (size_t i = 0; i < ENROLL_QUEUE; ++i) {
    EnrollJob &job = enrollQueue[i];
    if (!job.used) continue;

    Bulb *b = registryFindByIeee(job.ieee);
    if (b == nullptr) {
      job.used = false;  // The bulb was removed meanwhile.
      continue;
    }
    if (++job.attempts > ENROLL_MAX_ATTEMPTS) {
      job.used = false;  // Give up; the next online transition re-queues.
      continue;
    }
    if (!bulbReady(b)) continue;  // Not addressable yet; retry next pass.

    esp_zb_zcl_groups_add_group_cmd_t cmd = {};
    cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
    cmd.zcl_basic_cmd.dst_endpoint = b->endpoint;
    cmd.zcl_basic_cmd.dst_addr_u.addr_short = b->shortAddr;
    cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd.group_id = ZIGBEE_GROUP_ID;
    if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
    uint8_t err = esp_zb_zcl_groups_add_group_cmd_req(&cmd);
    esp_zb_lock_release();

    job.used = false;
    if (err == ESP_OK) {
      b->groupMember = true;  // Optimistic: re-sent on the next online pass.
      debugLogPrintf("Groups: %s joined group 0x%04x\n", b->name, ZIGBEE_GROUP_ID);
    }
    return;  // One enrollment per pass.
  }
}

void groupForget(Bulb *bulb) {
  if (bulb == nullptr || !bulbReady(bulb)) return;
  esp_zb_zcl_groups_add_group_cmd_t cmd = {};
  cmd.zcl_basic_cmd.src_endpoint = BULB_ENDPOINT;
  cmd.zcl_basic_cmd.dst_endpoint = bulb->endpoint;
  cmd.zcl_basic_cmd.dst_addr_u.addr_short = bulb->shortAddr;
  cmd.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
  cmd.group_id = ZIGBEE_GROUP_ID;
  if (!esp_zb_lock_acquire(portMAX_DELAY)) return;
  esp_zb_zcl_groups_remove_group_cmd_req(&cmd);
  esp_zb_lock_release();
}

bool groupSendOn() {
  sendGroupOnOff(true);
  return true;
}

bool groupSendOff() {
  sendGroupOnOff(false);
  return true;
}

bool groupSendBrightness(uint8_t pct, uint16_t transitionDs) {
  if (pct == 0) {
    sendGroupOnOff(false);
    return true;
  }
  const uint8_t level = (uint8_t)((pct * 255 + 50) / 100);
  sendGroupMoveToLevel(level, clampTransition(transitionDs));
  return true;
}

bool groupSendKelvin(int kelvin, uint16_t transitionDs) {
  sendGroupColorTemperature(kelvinToMireds(kelvin), clampTransition(transitionDs));
  return true;
}

bool groupSendRgb(uint8_t r, uint8_t g, uint8_t b, uint16_t transitionDs) {
  espXyColor_t xy = espRgbToXYColor(r, g, b);
  sendGroupMoveToColor(xy.x, xy.y, clampTransition(transitionDs));
  return true;
}
