// Tests for json_lite.h, the hand-rolled JSON reader shared by the REST API,
// the MQTT bridge and the backup module.

#include "check.h"
#include "json_lite.h"

namespace {

const size_t NONE = (size_t)-1;

void testFindJsonKey() {
  check::begin("findJsonKey");

  CHECK_EQ(findJsonKey("{\"a\":1}", "a"), (size_t)5);
  CHECK_EQ(findJsonKey("{ \"a\" :  1 }", "a"), (size_t)9);
  CHECK(findJsonKey("{\"a\":1}", "b") == NONE);       // missing key
  CHECK(findJsonKey("", "a") == NONE);                // empty body
  CHECK(findJsonKey("{\"name\":\"key\"}", "key") == NONE);  // "key" is a value
  CHECK(findJsonKey("{\"a\" 1}", "a") == NONE);       // key not followed by ':'
  CHECK_EQ(findJsonKey("{\n\t\"a\"\t:\n7}", "a"), (size_t)9);  // tab/newline whitespace

  // A value that looks like a later key must not shadow the real one.
  CHECK_EQ(findJsonKey("{\"s\":\"on\",\"on\":true}", "on"), (size_t)15);
}

void testJsonGetBool() {
  check::begin("jsonGetBool");

  bool v = false;
  CHECK(jsonGetBool("{\"on\":true}", "on", v) && v == true);
  CHECK(jsonGetBool("{\"on\":false}", "on", v) && v == false);
  CHECK(jsonGetBool("{\"on\": true}", "on", v) && v == true);
  CHECK(jsonGetBool("{\"a\":false,\"on\":true}", "on", v) && v == true);
  CHECK(!jsonGetBool("{\"on\":1}", "on", v));          // numbers are not booleans
  CHECK(!jsonGetBool("{\"on\":\"true\"}", "on", v));   // quoted text is not a boolean
  CHECK(!jsonGetBool("{\"other\":true}", "on", v));    // missing key
}

void testJsonGetInt() {
  check::begin("jsonGetInt");

  long v = 0;
  CHECK(jsonGetInt("{\"level\":50}", "level", v) && v == 50);
  CHECK(jsonGetInt("{\"level\": -7}", "level", v) && v == -7);
  CHECK(jsonGetInt("{\"n\":0}", "n", v) && v == 0);
  CHECK(jsonGetInt("{\"a\":1,\"n\":42}", "n", v) && v == 42);
  CHECK(jsonGetInt("{\"n\":12abc}", "n", v) && v == 12);  // leading digits win
  CHECK(!jsonGetInt("{\"n\":abc}", "n", v));
  CHECK(!jsonGetInt("{\"n\":-}", "n", v));                // lone minus is not a number
  CHECK(!jsonGetInt("{\"other\":1}", "n", v));            // missing key
}

void testJsonGetString() {
  check::begin("jsonGetString");

  String s;
  CHECK(jsonGetString("{\"name\":\"Salotto\"}", "name", s) && s == "Salotto");
  CHECK(jsonGetString("{\"name\": \"Cucina\" }", "name", s) && s == "Cucina");
  CHECK(jsonGetString("{\"name\":\"\"}", "name", s) && s.length() == 0);
  CHECK(!jsonGetString("{\"n\":42}", "n", s));  // not a string

  // Escapes are unwrapped.
  CHECK(jsonGetString("{\"s\":\"a\\\"b\"}", "s", s) && s == "a\"b");
  CHECK(jsonGetString("{\"s\":\"a\\\\b\"}", "s", s) && s == "a\\b");

  CHECK(!jsonGetString("{\"other\":\"x\"}", "s", s));  // missing key
}

void testRealisticBody() {
  check::begin("realistic API body");

  const char *body =
      "{\"power\":true,\"level\":75,\"kelvin\":2700,\"transition\":10,\"debug\":false}";
  bool power = false, debug = true;
  long level = 0, kelvin = 0, transition = 0;
  CHECK(jsonGetBool(body, "power", power) && power);
  CHECK(jsonGetInt(body, "level", level) && level == 75);
  CHECK(jsonGetInt(body, "kelvin", kelvin) && kelvin == 2700);
  CHECK(jsonGetInt(body, "transition", transition) && transition == 10);
  CHECK(jsonGetBool(body, "debug", debug) && debug == false);

  // A bulb named like a later key must not break parsing.
  bool p = false;
  CHECK(jsonGetBool("{\"name\":\"power\",\"power\":true}", "power", p) && p);
}

}  // namespace

void runJsonLiteTests() {
  testFindJsonKey();
  testJsonGetBool();
  testJsonGetInt();
  testJsonGetString();
  testRealisticBody();
}
