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

} // namespace

void runWebUtilsTests() {
  testValidHostnames();
  testInvalidHostnamesLength();
  testInvalidHostnamesHyphens();
  testInvalidHostnamesChars();
}
