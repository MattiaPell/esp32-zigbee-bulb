#include "check.h"
#include "shim/WString.h"
#include "config.h"

#define millis my_millis_web_utils
uint32_t my_mock_millis_web_utils = 0;
uint32_t my_millis_web_utils() { return my_mock_millis_web_utils; }

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#define registryRename registryRename_wu
#define bulbSendOff bulbSendOff_wu
#define bulbSendKelvin bulbSendKelvin_wu

#include "../bulb_registry.h"

// State tracking for tests
bool mock_bulb_send_on = false;
bool mock_bulb_send_off = false;
int mock_bulb_brightness = -1;
int mock_bulb_kelvin = -1;
int mock_bulb_red = -1;
int mock_bulb_green = -1;
int mock_bulb_blue = -1;
uint16_t mock_bulb_tds = 0;
bool mock_registry_rename = false;
String mock_rename_val = "";

void resetMocks() {
  mock_bulb_send_on = false;
  mock_bulb_send_off = false;
  mock_bulb_brightness = -1;
  mock_bulb_kelvin = -1;
  mock_bulb_red = -1;
  mock_bulb_green = -1;
  mock_bulb_blue = -1;
  mock_bulb_tds = 0;
  mock_registry_rename = false;
  mock_rename_val = "";
}

bool bulbReady(Bulb *b) { return b->online; }

void registryRename(Bulb *b, const char *name) {
  mock_registry_rename = true;
  mock_rename_val = name;
  strncpy(b->name, name, sizeof(b->name));
}

void bulbSendOn(Bulb *b) {
  b->state.power = true;
  mock_bulb_send_on = true;
}

void bulbSendOff(Bulb *b) {
  b->state.power = false;
  mock_bulb_send_off = true;
}

void bulbSendBrightness(Bulb *b, uint8_t level, uint16_t tds) {
  b->state.level = level;
  mock_bulb_brightness = level;
  mock_bulb_tds = tds;
}

void bulbSendKelvin(Bulb *b, int kelvin, uint16_t tds) {
  b->state.mode = BulbColorMode::White;
  b->state.kelvin = kelvin;
  mock_bulb_kelvin = kelvin;
  mock_bulb_tds = tds;
}

void bulbSendRgb(Bulb *b, uint8_t r, uint8_t g, uint8_t bl, uint16_t tds) {
  b->state.mode = BulbColorMode::Rgb;
  b->state.red = r;
  b->state.green = g;
  b->state.blue = bl;
  mock_bulb_red = r;
  mock_bulb_green = g;
  mock_bulb_blue = bl;
  mock_bulb_tds = tds;
}

// Directly include the target source file to test
#include "../web_utils.cpp"

namespace {

void testValidHostnames() {
  check::begin("isValidHostname: valid inputs");
  CHECK(isValidHostname("a"));
  CHECK(isValidHostname("1"));
  CHECK(isValidHostname("a1"));
  CHECK(isValidHostname("1a"));
  CHECK(isValidHostname("a-b"));
  CHECK(isValidHostname("esp32-zigbee"));
  CHECK(isValidHostname("esp32-zigbee-bulb"));
  CHECK(isValidHostname("1234567890123456789012345678901")); // 31 chars
}

void testInvalidHostnamesLength() {
  check::begin("isValidHostname: invalid length");
  CHECK(!isValidHostname("")); // empty
  CHECK(!isValidHostname("12345678901234567890123456789012")); // 32 chars
  CHECK(!isValidHostname("123456789012345678901234567890123")); // 33 chars
}

void testInvalidHostnamesHyphens() {
  check::begin("isValidHostname: invalid hyphens");
  CHECK(!isValidHostname("-a"));
  CHECK(!isValidHostname("a-"));
  CHECK(!isValidHostname("-a-"));
  CHECK(!isValidHostname("-"));
}

void testInvalidHostnamesChars() {
  check::begin("isValidHostname: invalid chars");
  CHECK(!isValidHostname("a_b"));
  CHECK(!isValidHostname("a b"));
  CHECK(!isValidHostname("A")); // uppercase not allowed in the actual implementation
  CHECK(!isValidHostname("esp32.local"));
  CHECK(!isValidHostname("a@b"));
}

void testParseRgbHex() {
  check::begin("parseRgbHex: valid strings");
  uint8_t r, g, b;

  CHECK(parseRgbHex("123456", r, g, b));
  CHECK_EQ((int)r, 0x12);
  CHECK_EQ((int)g, 0x34);
  CHECK_EQ((int)b, 0x56);

  CHECK(parseRgbHex("#abcdef", r, g, b));
  CHECK_EQ((int)r, 0xab);
  CHECK_EQ((int)g, 0xcd);
  CHECK_EQ((int)b, 0xef);

  CHECK(parseRgbHex("ABCDEF", r, g, b));
  CHECK_EQ((int)r, 0xAB);
  CHECK_EQ((int)g, 0xCD);
  CHECK_EQ((int)b, 0xEF);

  check::begin("parseRgbHex: error cases");
  CHECK(!parseRgbHex("", r, g, b)); // empty
  CHECK(!parseRgbHex("#", r, g, b)); // empty after hash
  CHECK(!parseRgbHex("12345", r, g, b)); // too short
  CHECK(!parseRgbHex("1234567", r, g, b)); // too long
  CHECK(!parseRgbHex("#1234567", r, g, b)); // too long with hash
  CHECK(!parseRgbHex("123 45", r, g, b)); // invalid chars (space)
  CHECK(!parseRgbHex("ZZZZZZ", r, g, b)); // invalid chars (not hex)
  CHECK(!parseRgbHex("-12345", r, g, b)); // invalid chars (-)
}

void testApplyLightPatch() {
  check::begin("applyLightPatch");
  Bulb b;
  PatchContext ctx;

  // offline bulb returns 409 for control keys
  b.online = false;
  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"on\":true}", ctx));
  CHECK_EQ(ctx.httpCode, 409);

  // rename works even offline
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"name\":\"Test\"}", ctx));
  CHECK(mock_registry_rename);
  CHECK_EQ(mock_rename_val, "Test");
  CHECK_EQ(ctx.httpCode, 200);

  b.online = true;

  // empty or no recognized fields -> 400
  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{}", ctx));
  CHECK_EQ(ctx.httpCode, 400);

  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"unknown\":1}", ctx));
  CHECK_EQ(ctx.httpCode, 400);

  // invalid name
  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"name\":\"\"}", ctx));
  CHECK_EQ(ctx.httpCode, 400);

  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"name\":\"123456789012345678901\"}", ctx)); // 21 chars
  CHECK_EQ(ctx.httpCode, 400);

  // on / off
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"on\":true}", ctx));
  CHECK(mock_bulb_send_on);
  CHECK(!mock_bulb_send_off);

  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"on\":false}", ctx));
  CHECK(!mock_bulb_send_on);
  CHECK(mock_bulb_send_off);

  // brightness
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"brightness\":50}", ctx));
  CHECK_EQ(mock_bulb_brightness, 50);
  CHECK_EQ(mock_bulb_tds, DEFAULT_TRANSITION_DS);

  // brightness constrained
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"brightness\":150}", ctx));
  CHECK_EQ(mock_bulb_brightness, 100);

  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"brightness\":-10}", ctx));
  CHECK_EQ(mock_bulb_brightness, 0);

  // kelvin
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"kelvin\":3000}", ctx));
  CHECK_EQ(mock_bulb_kelvin, 3000);

  // rgb_hex
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"rgb_hex\":\"FF0000\"}", ctx));
  CHECK_EQ(mock_bulb_red, 255);
  CHECK_EQ(mock_bulb_green, 0);
  CHECK_EQ(mock_bulb_blue, 0);

  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"rgb_hex\":\"#00FF00\"}", ctx));
  CHECK_EQ(mock_bulb_red, 0);
  CHECK_EQ(mock_bulb_green, 255);
  CHECK_EQ(mock_bulb_blue, 0);

  // invalid rgb_hex
  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"rgb_hex\":\"FF000\"}", ctx)); // too short
  CHECK_EQ(ctx.httpCode, 400);

  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"rgb_hex\":\"FF000G\"}", ctx)); // invalid char
  CHECK_EQ(ctx.httpCode, 400);

  // mode white
  b.state.kelvin = 4000;
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"mode\":\"white\"}", ctx));
  CHECK_EQ(mock_bulb_kelvin, 4000);

  // mode rgb
  b.state.red = 10;
  b.state.green = 20;
  b.state.blue = 30;
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"mode\":\"rgb\"}", ctx));
  CHECK_EQ(mock_bulb_red, 10);
  CHECK_EQ(mock_bulb_green, 20);
  CHECK_EQ(mock_bulb_blue, 30);

  // invalid mode
  resetMocks();
  ctx = PatchContext();
  CHECK(!applyLightPatch(&b, "{\"mode\":\"unknown\"}", ctx));
  CHECK_EQ(ctx.httpCode, 400);

  // transition override
  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"brightness\":50,\"transition\":0}", ctx));
  CHECK_EQ(mock_bulb_brightness, 50);
  CHECK_EQ(mock_bulb_tds, 0);

  resetMocks();
  ctx = PatchContext();
  CHECK(applyLightPatch(&b, "{\"brightness\":50,\"transition\":7000}", ctx));
  CHECK_EQ(mock_bulb_brightness, 50);
  CHECK_EQ(mock_bulb_tds, 6000); // max 6000
}

} // namespace

void runWebUtilsTests() {
  testValidHostnames();
  testInvalidHostnamesLength();
  testInvalidHostnamesHyphens();
  testInvalidHostnamesChars();
  testParseRgbHex();
  testApplyLightPatch();
}
