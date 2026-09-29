#include "check.h"
#include <Arduino.h>
#include "parse_rgb_payload.h"

// Define a stub for constrain if it's missing (though Arduino.h might provide it)
#ifndef constrain
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#endif

void runMqttBridgeTests() {
  check::begin("parseRgbPayload");

  uint8_t r = 0, g = 0, b = 0;

  // 1. Happy path: valid RGB JSON payload
  CHECK(parseRgbPayload("{\"color\":{\"r\":255,\"g\":128,\"b\":64}}", r, g, b));
  CHECK_EQ(r, 255);
  CHECK_EQ(g, 128);
  CHECK_EQ(b, 64);

  // 2. Missing "color" key
  CHECK(!parseRgbPayload("{\"r\":255,\"g\":128,\"b\":64}", r, g, b));

  // 3. Missing brace after "color"
  CHECK(!parseRgbPayload("{\"color\":\"red\"}", r, g, b));

  // 4. Missing one of the components
  CHECK(!parseRgbPayload("{\"color\":{\"r\":255,\"g\":128}}", r, g, b));

  // 5. Out of bounds components (should be constrained)
  CHECK(parseRgbPayload("{\"color\":{\"r\":300,\"g\":-10,\"b\":64}}", r, g, b));
  CHECK_EQ(r, 255);
  CHECK_EQ(g, 0);
  CHECK_EQ(b, 64);

  // 6. Valid components with surrounding whitespace
  CHECK(parseRgbPayload("{\"color\": { \"r\": 10, \"g\": 20, \"b\": 30 } }", r, g, b));
  CHECK_EQ(r, 10);
  CHECK_EQ(g, 20);
  CHECK_EQ(b, 30);

  // 7. Embedded within a larger MQTT payload
  CHECK(parseRgbPayload("{\"state\":\"ON\",\"brightness\":128,\"color\":{\"r\":100,\"g\":200,\"b\":50}}", r, g, b));
  CHECK_EQ(r, 100);
  CHECK_EQ(g, 200);
  CHECK_EQ(b, 50);
}
