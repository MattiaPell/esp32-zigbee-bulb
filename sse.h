#pragma once

// ---------------------------------------------------------------------------
// sse: pure helpers for the Server-Sent Events change stream (header-only,
// host-testable). The stream carries no state, only a change counter: the web
// page reacts to an event by fetching the REST API once.
// ---------------------------------------------------------------------------

#include <Arduino.h>

#include "web_auth.h"

inline uint32_t fnv1aAdd(uint32_t h, const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < len; ++i) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

constexpr uint32_t FNV1A_SEED = 2166136261u;

struct SseRequest {
  bool isStream = false;  // "GET /events"
  String host;
  String origin;
};

// Parses a request head (everything up to the blank line).
inline SseRequest parseSseRequest(const String &head) {
  SseRequest r;
  int lineStart = 0;
  bool first = true;
  while (lineStart < (int)head.length()) {
    int lineEnd = head.indexOf("\r\n", lineStart);
    if (lineEnd < 0) lineEnd = head.length();
    String line = head.substring(lineStart, lineEnd);
    lineStart = lineEnd + 2;
    if (first) {
      first = false;
      r.isStream = line.startsWith("GET /events ") || line.startsWith("GET /events?");
      continue;
    }
    const int colon = line.indexOf(':');
    if (colon <= 0) continue;
    String name = line.substring(0, colon);
    name.toLowerCase();
    String value = line.substring(colon + 1);
    value.trim();
    if (name == "host") r.host = value;
    else if (name == "origin") r.origin = value;
  }
  return r;
}

// Same Host / Origin policy as the REST API (web_auth.h). A request without
// Origin (curl, same-origin) is fine: it only ever sees a counter.
inline bool sseRequestAllowed(const SseRequest &r, const String &hostname,
                              const String &ip) {
  if (!r.isStream) return false;
  if (r.host.length() > 0 && !originOrHostAllowed(r.host, "", hostname, ip)) return false;
  if (r.origin.length() > 0 && !originOrHostAllowed(r.origin, "http://", hostname, ip)) return false;
  return true;
}

// Response head. CORS is granted only to the (already validated) Origin: the
// page is served on port 80 and the stream on another port.
inline String sseResponseHead(const String &allowedOrigin) {
  String h = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
             "Cache-Control: no-cache\r\nConnection: keep-alive\r\n"
             "X-Accel-Buffering: no\r\n";
  if (allowedOrigin.length() > 0) {
    h += "Access-Control-Allow-Origin: ";
    h += allowedOrigin;
    h += "\r\nVary: Origin\r\n";
  }
  h += "\r\n";
  return h;
}

inline String sseEventFrame(uint32_t version) {
  String f = "data: ";
  f += (unsigned long)version;
  f += "\n\n";
  return f;
}

inline const char *sseRetryFrame() { return "retry: 3000\n\n"; }

inline const char *sseHeartbeatFrame() { return ": hb\n\n"; }
