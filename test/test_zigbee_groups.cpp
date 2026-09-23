#include "check.h"
#include <cstdint>
#include <cstring>

extern uint32_t mock_millis;

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "zigbee_bulbs.h"
#include "zigbee_utils.h"

extern Bulb mockBulbs[3];
extern size_t mockBulbCount;

bool bulbReady(const Bulb *b) {
  return b != nullptr && b->online;
}

// Zigbee mocks
int lock_calls = 0;
int unlock_calls = 0;
bool esp_zb_lock_acquire(uint32_t /*ticks*/) {
  lock_calls++;
  return true;
}
void esp_zb_lock_release() {
  unlock_calls++;
}

#define portMAX_DELAY 0xFFFFFFFF
#define ESP_OK 0

typedef struct {
  uint8_t src_endpoint;
  uint8_t dst_endpoint;
  union { uint16_t addr_short; } dst_addr_u;
} esp_zb_zcl_basic_cmd_t;

typedef struct {
  esp_zb_zcl_basic_cmd_t zcl_basic_cmd;
  uint8_t address_mode;
  uint16_t group_id;
} esp_zb_zcl_groups_add_group_cmd_t;

typedef struct {
  esp_zb_zcl_basic_cmd_t zcl_basic_cmd;
  uint8_t address_mode;
  uint8_t on_off_cmd_id;
} esp_zb_zcl_on_off_cmd_t;

typedef struct {
  esp_zb_zcl_basic_cmd_t zcl_basic_cmd;
  uint8_t address_mode;
  uint8_t level;
  uint16_t transition_time;
} esp_zb_zcl_move_to_level_cmd_t;

typedef struct {
  esp_zb_zcl_basic_cmd_t zcl_basic_cmd;
  uint8_t address_mode;
  uint16_t color_temperature;
  uint16_t transition_time;
} esp_zb_zcl_color_move_to_color_temperature_cmd_t;

typedef struct {
  esp_zb_zcl_basic_cmd_t zcl_basic_cmd;
  uint8_t address_mode;
  uint16_t color_x;
  uint16_t color_y;
  uint16_t transition_time;
} esp_zb_zcl_color_move_to_color_cmd_t;

#define ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT 0x01
#define ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT 0x02
#define ESP_ZB_ZCL_CMD_ON_OFF_ON_ID 0x01
#define ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID 0x00

int add_group_calls = 0;
uint8_t esp_zb_zcl_groups_add_group_cmd_req(esp_zb_zcl_groups_add_group_cmd_t* /*cmd*/) {
  add_group_calls++;
  return ESP_OK;
}

int remove_group_calls = 0;
void esp_zb_zcl_groups_remove_group_cmd_req(esp_zb_zcl_groups_add_group_cmd_t* /*cmd*/) {
  remove_group_calls++;
}

int on_off_req_calls = 0;
void esp_zb_zcl_on_off_cmd_req(esp_zb_zcl_on_off_cmd_t* /*cmd*/) {
  on_off_req_calls++;
}

int move_to_level_calls = 0;
void esp_zb_zcl_level_move_to_level_with_onoff_cmd_req(esp_zb_zcl_move_to_level_cmd_t* /*cmd*/) {
  move_to_level_calls++;
}

int color_temp_calls = 0;
void esp_zb_zcl_color_move_to_color_temperature_cmd_req(esp_zb_zcl_color_move_to_color_temperature_cmd_t* /*cmd*/) {
  color_temp_calls++;
}

int move_to_color_calls = 0;
void esp_zb_zcl_color_move_to_color_cmd_req(esp_zb_zcl_color_move_to_color_cmd_t* /*cmd*/) {
  move_to_color_calls++;
}

typedef struct { uint16_t x; uint16_t y; } espXyColor_t;
espXyColor_t espRgbToXYColor(uint8_t r, uint8_t g, uint8_t /*b*/) { return {r, g}; }

#include "../zigbee_groups.cpp"

namespace {

void resetMocks() {
  mock_millis = 10000;
  lock_calls = 0;
  unlock_calls = 0;
  add_group_calls = 0;
  remove_group_calls = 0;
  on_off_req_calls = 0;
  move_to_level_calls = 0;
  color_temp_calls = 0;
  move_to_color_calls = 0;

  lastEnrollMs = 0;
  for (size_t i = 0; i < ENROLL_QUEUE; ++i) {
    enrollQueue[i].used = false;
    enrollQueue[i].attempts = 0;
  }

  for (int i = 0; i < 3; ++i) {
    mockBulbs[i] = Bulb();
    mockBulbs[i].ieee[0] = i + 1;
    mockBulbs[i].online = true;
    mockBulbs[i].groupMember = false;
    snprintf(mockBulbs[i].name, sizeof(mockBulbs[i].name), "MockBulb%d", i);
  }
}

void testGroupEnroll() {
  check::begin("zigbee groups: group enroll");
  resetMocks();

  Bulb* b = &mockBulbs[0];

  // Null check
  CHECK(groupEnroll(nullptr));

  // Already a member
  b->groupMember = true;
  CHECK(groupEnroll(b));
  b->groupMember = false; // Reset

  // Add to queue
  CHECK(groupEnroll(b));
  // Find which one is used
  int idx = -1;
  for(size_t i=0; i<ENROLL_QUEUE; ++i) { if (enrollQueue[i].used) { idx = i; break; } }
  CHECK(idx != -1);
  if (idx != -1) {
      CHECK_EQ(enrollQueue[idx].ieee[0], b->ieee[0]);
  }

  // Already queued
  CHECK(groupEnroll(b));
  // Queue should still only have 1 item
  int count = 0;
  for (size_t i = 0; i < ENROLL_QUEUE; ++i) {
    if (enrollQueue[i].used) count++;
  }
  CHECK_EQ(count, 1);

  // Fill the queue
  for (size_t i = 1; i < ENROLL_QUEUE; ++i) {
      Bulb newBulb = Bulb();
      newBulb.ieee[0] = i + 10;
      CHECK(groupEnroll(&newBulb));
  }

  // Queue full
  Bulb extraBulb = Bulb();
  extraBulb.ieee[0] = 99;
  CHECK(!groupEnroll(&extraBulb));
}

void testGroupTick() {
  check::begin("zigbee groups: group tick");
  resetMocks();

  Bulb* b = &mockBulbs[0];
  groupEnroll(b);

  // Tick before interval
  groupTick(0);
  CHECK_EQ(add_group_calls, 0);

  // Tick at/after interval
  groupTick(ENROLL_INTERVAL_MS);
  CHECK_EQ(add_group_calls, 1);
  CHECK_EQ(lock_calls, 1);
  CHECK_EQ(unlock_calls, 1);
  CHECK(b->groupMember);

  // It shouldn't be used anymore
  int idx = -1;
  for(size_t i=0; i<ENROLL_QUEUE; ++i) { if (enrollQueue[i].ieee[0] == b->ieee[0]) { idx = i; break; } }
  if (idx != -1) {
      CHECK(!enrollQueue[idx].used);
  }

  // Tick with unreachable bulb
  resetMocks();
  Bulb* b2 = &mockBulbs[1];
  b2->online = false;
  groupEnroll(b2);

  groupTick(ENROLL_INTERVAL_MS);
  CHECK_EQ(add_group_calls, 0);

  idx = -1;
  for(size_t i=0; i<ENROLL_QUEUE; ++i) { if (enrollQueue[i].ieee[0] == b2->ieee[0]) { idx = i; break; } }
  if (idx != -1) {
      CHECK(enrollQueue[idx].used);
      CHECK_EQ(enrollQueue[idx].attempts, 1);
  }

  // Tick until max attempts
  for (int i = 0; i < ENROLL_MAX_ATTEMPTS; ++i) {
      groupTick(ENROLL_INTERVAL_MS * (i + 2));
  }
  if (idx != -1) {
      CHECK(!enrollQueue[idx].used); // Dropped from queue
  }

  // Tick with removed bulb
  resetMocks();
  Bulb tempBulb = Bulb();
  tempBulb.ieee[0] = 99; // Not in registry mock
  groupEnroll(&tempBulb);
  groupTick(ENROLL_INTERVAL_MS);
  idx = -1;
  for(size_t i=0; i<ENROLL_QUEUE; ++i) { if (enrollQueue[i].ieee[0] == tempBulb.ieee[0]) { idx = i; break; } }
  if (idx != -1) {
      CHECK(!enrollQueue[idx].used); // Dropped from queue
  }
}

void testGroupForget() {
  check::begin("zigbee groups: group forget");
  resetMocks();

  Bulb* b = &mockBulbs[0];

  // Null check
  groupForget(nullptr);
  CHECK_EQ(remove_group_calls, 0);

  // Not ready check
  b->online = false;
  groupForget(b);
  CHECK_EQ(remove_group_calls, 0);

  // Forget
  b->online = true;
  groupForget(b);
  CHECK_EQ(remove_group_calls, 1);
  CHECK_EQ(lock_calls, 1);
  CHECK_EQ(unlock_calls, 1);
}

void testGroupSends() {
  check::begin("zigbee groups: group sends");
  resetMocks();

  groupSendOn();
  CHECK_EQ(on_off_req_calls, 1);
  CHECK_EQ(lock_calls, 1);

  groupSendOff();
  CHECK_EQ(on_off_req_calls, 2);
  CHECK_EQ(lock_calls, 2);

  groupSendBrightness(0, 10);
  CHECK_EQ(on_off_req_calls, 3);
  CHECK_EQ(lock_calls, 3);

  groupSendBrightness(50, 10);
  CHECK_EQ(move_to_level_calls, 1);
  CHECK_EQ(lock_calls, 4);

  groupSendKelvin(3000, 10);
  CHECK_EQ(color_temp_calls, 1);
  CHECK_EQ(lock_calls, 5);

  groupSendRgb(255, 0, 0, 10);
  CHECK_EQ(move_to_color_calls, 1);
  CHECK_EQ(lock_calls, 6);
}

void testKelvinToMireds() {
    check::begin("zigbee groups: kelvin to mireds");
    CHECK_EQ(kelvinToMireds(1000), kelvinToMireds(MIN_KELVIN)); // clamped
    CHECK_EQ(kelvinToMireds(5000), kelvinToMireds(MAX_KELVIN)); // clamped
    CHECK_EQ(kelvinToMireds(3000), (uint16_t)(1000000UL / 3000));
}

} // namespace

void runZigbeeGroupsTests() {
  testGroupEnroll();
  testGroupTick();
  testGroupForget();
  testGroupSends();
  testKelvinToMireds();
}
