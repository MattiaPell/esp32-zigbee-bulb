#include "check.h"
#include "zigbee_utils.h"

void runZigbeeUtilsTests() {
  check::begin("zigbee utils");

  CHECK_EQ(clampTransition(0), 0);
  CHECK_EQ(clampTransition(50), 50);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS - 1), MAX_TRANSITION_DS - 1);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS), MAX_TRANSITION_DS);
  CHECK_EQ(clampTransition(MAX_TRANSITION_DS + 1), MAX_TRANSITION_DS);
  CHECK_EQ(clampTransition(65535), MAX_TRANSITION_DS); // UINT16_MAX
}
