#include "check.h"
#include "shim/WString.h"
#include "config.h"

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

} // namespace

void runWebUtilsTests() {
  testValidHostnames();
  testInvalidHostnamesLength();
  testInvalidHostnamesHyphens();
  testInvalidHostnamesChars();
  testParseRgbHex();
}
