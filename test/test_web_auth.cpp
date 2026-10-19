#include "check.h"
#include "shim/WString.h"
#include "../web_auth.h"

void runWebAuthTests() {
  check::begin("web auth");

  CHECK(tokenMatches(String("secret"), "secret"));
  CHECK(!tokenMatches(String("secreT"), "secret"));
  CHECK(!tokenMatches(String("secre"), "secret"));
  CHECK(!tokenMatches(String("secret2"), "secret"));
  CHECK(!tokenMatches(String(""), "secret"));

  const String host("bulb"), ip("192.168.1.50");
  auto req = [&](bool get, const char *h, const char *o, bool tok) {
    return requestAuthorized(get, false, String(h), String(o), tok, host, ip);
  };

  // Host allow-list (also on GET).
  CHECK(req(true, "bulb.local", "", false));
  CHECK(req(true, "bulb.local:80", "", false));
  CHECK(req(true, "192.168.1.50", "", false));
  CHECK(req(true, "localhost:8080", "", false));
  CHECK(req(true, "", "", false));
  CHECK(!req(true, "evil.example.com", "", false));
  CHECK(!req(true, "bulb.local.evil.com", "", false));

  // Writes from a browser: Origin must be ours.
  CHECK(req(false, "bulb.local", "http://bulb.local", false));
  CHECK(req(false, "192.168.1.50", "http://192.168.1.50:80", false));
  CHECK(!req(false, "bulb.local", "http://evil.example.com", false));
  CHECK(!req(false, "bulb.local", "http://evil.example.com", true));
  CHECK(!req(false, "bulb.local", "null", true));
  CHECK(!req(false, "bulb.local", "http://bulb.local.evil.com", false));

  // Writes without Origin (curl, scripts): only with a valid token.
  CHECK(!req(false, "bulb.local", "", false));
  CHECK(req(false, "bulb.local", "", true));
  CHECK(req(false, "", "", true));
  CHECK(!req(false, "evil.example.com", "", true));

  CHECK(requestAuthorized(false, true, String("bulb.local"), String(""), false, host, ip));
}
