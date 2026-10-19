#include "check.h"
#include "shim/WString.h"
#include "../sse.h"

void runSseTests() {
  check::begin("sse");

  // FNV-1a known vector: "a" -> 0xE40C292C.
  CHECK(fnv1aAdd(FNV1A_SEED, "a", 1) == 0xE40C292Cu);
  CHECK(fnv1aAdd(FNV1A_SEED, "a", 1) != fnv1aAdd(FNV1A_SEED, "b", 1));

  const String head("GET /events HTTP/1.1\r\nHost: bulb.local:81\r\n"
                    "Origin: http://bulb.local\r\nAccept: text/event-stream\r\n\r\n");
  SseRequest r = parseSseRequest(head);
  CHECK(r.isStream);
  CHECK(r.host == "bulb.local:81");
  CHECK(r.origin == "http://bulb.local");

  const String hostname("bulb"), ip("192.168.1.50");
  CHECK(sseRequestAllowed(r, hostname, ip));

  // Header names are case-insensitive.
  r = parseSseRequest(String("GET /events HTTP/1.1\r\nhost: 192.168.1.50:81\r\n"
                             "ORIGIN: http://192.168.1.50\r\n\r\n"));
  CHECK(sseRequestAllowed(r, hostname, ip));

  // No Origin (curl): allowed, and no CORS header is granted.
  r = parseSseRequest(String("GET /events HTTP/1.1\r\nHost: bulb.local:81\r\n\r\n"));
  CHECK(sseRequestAllowed(r, hostname, ip));
  CHECK(sseResponseHead(r.origin).indexOf("Access-Control") < 0);

  // Foreign Origin / Host (CSRF-style subscribe, DNS rebinding) is refused.
  r = parseSseRequest(String("GET /events HTTP/1.1\r\nHost: bulb.local:81\r\n"
                             "Origin: http://evil.example.com\r\n\r\n"));
  CHECK(!sseRequestAllowed(r, hostname, ip));
  r = parseSseRequest(String("GET /events HTTP/1.1\r\nHost: evil.example.com\r\n\r\n"));
  CHECK(!sseRequestAllowed(r, hostname, ip));

  // Only GET /events is a stream.
  r = parseSseRequest(String("GET /api/lights HTTP/1.1\r\nHost: bulb.local\r\n\r\n"));
  CHECK(!r.isStream);
  CHECK(!sseRequestAllowed(r, hostname, ip));
  r = parseSseRequest(String("POST /events HTTP/1.1\r\nHost: bulb.local\r\n\r\n"));
  CHECK(!r.isStream);
  r = parseSseRequest(String("GET /events?x=1 HTTP/1.1\r\nHost: bulb.local\r\n\r\n"));
  CHECK(r.isStream);

  const String resp = sseResponseHead("http://bulb.local");
  CHECK(resp.indexOf("text/event-stream") >= 0);
  CHECK(resp.indexOf("Access-Control-Allow-Origin: http://bulb.local\r\n") >= 0);
  CHECK(resp.endsWith("\r\n\r\n"));

  CHECK(sseEventFrame(42) == "data: 42\n\n");
}
