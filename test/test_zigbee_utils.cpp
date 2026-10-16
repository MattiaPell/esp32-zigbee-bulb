#include "check.h"
#include "test/shim/Zigbee.h"
#include "zigbee_utils.h"

void runZigbeeUtilsTests() {
  check::begin("zigbee utils");

  CHECK_EQ(clampTransition(0), 0);
  CHECK_EQ(clampTransition(50), 50);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS - 1), MAX_TRANSITION_DS - 1);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS), MAX_TRANSITION_DS);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS + 1), MAX_TRANSITION_DS);
  CHECK_EQ(clampTransition(65535), MAX_TRANSITION_DS); // UINT16_MAX

  esp_zb_ieee_addr_t list[3] = {
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
    {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18},
    {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28}
  };

  esp_zb_ieee_addr_t match1 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  esp_zb_ieee_addr_t match2 = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28};
  esp_zb_ieee_addr_t no_match1 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  esp_zb_ieee_addr_t no_match2 = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09}; // Almost match1

  CHECK(ieeeListContains(list, 3, match1));
  CHECK(ieeeListContains(list, 3, match2));
  CHECK(!ieeeListContains(list, 3, no_match1));
  CHECK(!ieeeListContains(list, 3, no_match2));

  // Test count limits
  CHECK(!ieeeListContains(list, 0, match1)); // Count 0 should return false
  CHECK(!ieeeListContains(list, 2, match2)); // match2 is at index 2, should not be found if count is 2
}
