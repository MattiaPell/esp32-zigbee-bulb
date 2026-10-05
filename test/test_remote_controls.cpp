#include "check.h"
#include <cstdint>

#define millis millis_remote_controls
#define debugLogPrintf debugLogPrintf_remote_controls
#define esp_zb_lock_acquire esp_zb_lock_acquire_rc
#define esp_zb_lock_release esp_zb_lock_release_rc

uint32_t mock_millis_remote_controls = 0;
uint32_t millis() { return mock_millis_remote_controls; }
void debugLogPrintf(const char * /*format*/, ...) {}

#include "shim/WString.h"
#include "shim/Preferences.h"
#include "shim/Zigbee.h"

// Define stubs for dependencies

class ZigbeeMock {
public:
  bool started() { return true; }
  bool connected() { return true; }
};
ZigbeeMock Zigbee;

// Mocks for Zigbee structures
#define ESP_ZB_ZCL_ADDR_TYPE_SHORT 0x02
typedef struct { uint16_t short_addr; } esp_zb_zcl_addr_u_t;
typedef struct { int addr_type; esp_zb_zcl_addr_u_t u; } esp_zb_zcl_addr_t;
typedef struct { uint8_t id; } esp_zb_zcl_cmd_t;
typedef struct { int status; esp_zb_zcl_addr_t src_address; uint8_t src_endpoint; uint16_t cluster; esp_zb_zcl_cmd_t command; } esp_zb_zcl_cmd_info_t;
typedef struct { esp_zb_zcl_cmd_info_t info; const uint8_t* data; int size; } esp_zb_zcl_privilege_command_message_t;
typedef struct { esp_zb_zcl_cmd_info_t info; const uint8_t* data; int size; } esp_zb_zcl_custom_cluster_command_message_t;
#define ESP_ZB_ZCL_STATUS_SUCCESS 0

typedef struct { int dst_nwk_addr; uint16_t addr_of_interest; int request_type; int start_index; } esp_zb_zdo_ieee_addr_req_param_t;
typedef struct { uint16_t nwk_addr; esp_zb_ieee_addr_t ieee_addr; } esp_zb_zdo_ieee_addr_rsp_t;
typedef int esp_zb_zdp_status_t;
#define ESP_ZB_ZDP_STATUS_SUCCESS 0

typedef void (*esp_zb_zdo_ieee_addr_cb_t)(esp_zb_zdp_status_t status, esp_zb_zdo_ieee_addr_rsp_t* resp, void* user_cb);
void esp_zb_zdo_ieee_addr_req(esp_zb_zdo_ieee_addr_req_param_t* /*req*/, esp_zb_zdo_ieee_addr_cb_t /*cb*/, void* /*user_ctx*/) {}
bool esp_zb_lock_acquire_rc(uint32_t) { return true; }
void esp_zb_lock_release_rc() {}

#define portMAX_DELAY 0xFFFFFFFF

struct Bulb; // Forward declaration

class ZigbeeEP {
public:
  void onPrivilegeCommand(void (*)(const esp_zb_zcl_privilege_command_message_t*)) {}
  void onCustomClusterCommand(void (*)(const esp_zb_zcl_custom_cluster_command_message_t*)) {}
  void addPrivilegeCommand(uint16_t, uint8_t) {}
};

#include "../bulb_registry.h" // We need the actual Bulb struct to avoid compilation issues

// Mocks for bulb_registry.h
#define registryCount mock_registryCount_rc
#define registryGet mock_registryGet_rc
#define bulbReady mock_bulbReady_rc
#define bulbBrightnessPct mock_bulbBrightnessPct_rc
#define bulbSendAllOn mock_bulbSendAllOn_rc
#define bulbSendAllOff mock_bulbSendAllOff_rc
#define bulbSendBrightness mock_bulbSendBrightness_rc
#define bulbIeeeHex mock_bulbIeeeHex_rc

size_t mock_registryCount_rc() { return 0; }
Bulb* mock_registryGet_rc(size_t) { return nullptr; }
bool mock_bulbReady_rc(const Bulb*) { return false; }
uint8_t mock_bulbBrightnessPct_rc(const Bulb*) { return 0; }
int mock_bulbSendAllOn_rc() { return 1; }
int mock_bulbSendAllOff_rc() { return 1; }
void mock_bulbSendBrightness_rc(Bulb*, uint8_t, uint16_t) {}
String mock_bulbIeeeHex_rc(const Bulb*) { return "0011223344556677"; }

// Mocks for scenes.h
#define scenesCount mock_scenesCount_rc
#define sceneNameAt mock_sceneNameAt_rc
#define sceneRecall mock_sceneRecall_rc

size_t mock_scenesCount_rc() { return 0; }
String mock_sceneNameAt_rc(size_t) { return "scene"; }
bool mock_sceneRecall_rc(const String&, int&, int&) { return true; }

// Mocks for zigbee_bulbs.h
#define zigbeeMemberSnapshot mock_zigbeeMemberSnapshot_rc
#define zigbeeBoundSnapshot mock_zigbeeBoundSnapshot_rc
#define zigbeeQueueRemoteBind mock_zigbeeQueueRemoteBind_rc
#define zigbeeRemoteBindBusy mock_zigbeeRemoteBindBusy_rc

#include "../zigbee_bulbs.h"

size_t mock_zigbeeMemberSnapshot_rc(DeviceInfo*, size_t) { return 0; }
size_t mock_zigbeeBoundSnapshot_rc(DeviceInfo*, size_t) { return 0; }
bool mock_zigbeeQueueRemoteBind_rc(const esp_zb_ieee_addr_t, uint16_t, uint8_t, Bulb*) { return true; }
bool mock_zigbeeRemoteBindBusy_rc() { return false; }

// Mocks for web_hooks.h
#define webHookEvent mock_webHookEvent_rc
bool mock_webHookEvent_rc(const char*, const char*, const char*, const char*) { return true; }

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

// We don't want unused parameter warnings from executeAction which we don't care about here
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../remote_controls.cpp"
#pragma GCC diagnostic pop


void testNormalizeEvent() {
  check::begin("remote_controls: normalizeEvent");

  // On/Off
  CHECK_EQ(normalizeEvent(0x0006, 0x00, nullptr, 0), EV_OFF);
  CHECK_EQ(normalizeEvent(0x0006, 0x01, nullptr, 0), EV_ON);
  CHECK_EQ(normalizeEvent(0x0006, 0x02, nullptr, 0), EV_TOGGLE);

  // Level control
  uint8_t moveUpPayload[] = {0x00};
  uint8_t moveDownPayload[] = {0x01};
  CHECK_EQ(normalizeEvent(0x0008, 0x01, moveUpPayload, 1), EV_MOVE_UP);
  CHECK_EQ(normalizeEvent(0x0008, 0x01, moveDownPayload, 1), EV_MOVE_DOWN);
  CHECK_EQ(normalizeEvent(0x0008, 0x02, moveUpPayload, 1), EV_STEP_UP);
  CHECK_EQ(normalizeEvent(0x0008, 0x03, nullptr, 0), EV_STOP);

  // Scene Recall
  CHECK_EQ(normalizeEvent(0x0005, 0x05, nullptr, 0), EV_SCENE);

  // Unknown
  CHECK_EQ(normalizeEvent(0x9999, 0x99, nullptr, 0), EV_COUNT);
}

void testRemoteActionsSet() {
  check::begin("remote_controls: remoteSetActions");

  String applied;
  // valid json
  bool ok = remoteSetActions("{\"on\":\"all_off\", \"off\":\"all_on\"}", applied);
  CHECK(ok);
  CHECK_EQ(actionMap[EV_ON], ACT_ALL_OFF);
  CHECK_EQ(actionMap[EV_OFF], ACT_ALL_ON);

  // invalid action name
  ok = remoteSetActions("{\"toggle\":\"dance\"}", applied);
  CHECK(!ok);

  // ignore invalid event name
  ok = remoteSetActions("{\"dance\":\"all_on\", \"step_up\":\"brightness_down\"}", applied);
  CHECK(ok);
  CHECK_EQ(actionMap[EV_STEP_UP], ACT_DIM_DOWN);
}

void testRemoteActionsGet() {
  check::begin("remote_controls: remoteActionsJson");

  // Default values
  String json = remoteActionsJson();
  CHECK(json.indexOf("\"off\":\"all_off\"") >= 0);
  CHECK(json.indexOf("\"on\":\"all_on\"") >= 0);
  CHECK(json.indexOf("\"toggle\":\"toggle_all\"") >= 0);

  // Alter value
  String applied;
  remoteSetActions("{\"off\":\"brightness_up\"}", applied);
  json = remoteActionsJson();
  CHECK(json.indexOf("\"off\":\"brightness_up\"") >= 0);
}

void testRemoteEnroll() {
  check::begin("remote_controls: remoteEnroll");

  esp_zb_ieee_addr_t ieee1 = {1, 2, 3, 4, 5, 6, 7, 8};
  esp_zb_ieee_addr_t ieee2 = {8, 7, 6, 5, 4, 3, 2, 1};

  // Add first remote
  bool enrolled1 = remoteEnroll(ieee1, 0x1111, 1);
  CHECK(enrolled1);
  CHECK_EQ((int)remoteCount(), 1);

  // Find by ID
  String id1 = remoteIdAt(0);
  CHECK(id1.length() > 0);

  // Add second remote
  bool enrolled2 = remoteEnroll(ieee2, 0x2222, 1);
  CHECK(enrolled2);
  CHECK_EQ((int)remoteCount(), 2);

  // Rename second remote
  String id2 = remoteIdAt(1);
  bool renamed = remoteRename(id2, "Living Room Remote");
  CHECK(renamed);

  // List JSON
  String list = remoteListJson();
  CHECK(list.indexOf("Living Room Remote") >= 0);
  CHECK(list.indexOf("0x1111") >= 0);
  CHECK(list.indexOf("0x2222") >= 0);

  // Remove first remote
  bool removed = remoteRemove(id1);
  CHECK(removed);
  CHECK_EQ((int)remoteCount(), 1);
}

void runRemoteControlsTests() {
  testRemoteEnroll();
  testRemoteActionsGet();
  testNormalizeEvent();
  testRemoteActionsSet();
}
