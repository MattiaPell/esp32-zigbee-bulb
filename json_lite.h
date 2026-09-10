#pragma once

// ---------------------------------------------------------------------------
// json_lite: the hand-rolled JSON dialect shared by the REST API, the MQTT
// bridge and the backup module. Flat objects with string / number / boolean
// values; no nesting, no arrays, no escaping guarantees beyond the basic
// skipped escapes in strings.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Find "key": in body; returns the value start position or (size_t)-1.
inline size_t findJsonKey(const String &body, const char *key) {
  // Tolerant to spaced-out JSON: find "key", skip whitespace, expect ':'. A
  // string value that merely equals the key name (e.g. a bulb called "power")
  // must not shadow the real key: keep scanning later occurrences.
  String needle = "\"";
  needle += key;
  needle += "\"";
  unsigned int from = 0;
  while (true) {
    const int at = body.indexOf(needle, from);
    if (at < 0) return (size_t)-1;
    size_t pos = (size_t)at + needle.length();
    while (pos < body.length() && isspace((unsigned char)body[pos])) ++pos;
    if (pos < body.length() && body[pos] == ':') {
      ++pos;
      while (pos < body.length() && isspace((unsigned char)body[pos])) ++pos;
      return pos;
    }
    from = (unsigned int)at + 1;  // Not a key: try the next occurrence.
  }
}

inline bool jsonGetBool(const String &body, const char *key, bool &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1) return false;
  if (body.indexOf("true", at) == (int)at) {
    out = true;
    return true;
  }
  if (body.indexOf("false", at) == (int)at) {
    out = false;
    return true;
  }
  return false;
}

inline bool jsonGetInt(const String &body, const char *key, long &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1) return false;
  size_t start = at, end = at;
  const char *s = body.c_str();
  if (s[start] == '-') ++end;
  while (end < body.length() && s[end] >= '0' && s[end] <= '9') ++end;
  if (end == start || (end == start + 1 && s[start] == '-')) return false;
  out = body.substring(start, end).toInt();
  return true;
}

inline bool jsonGetString(const String &body, const char *key, String &out) {
  size_t at = findJsonKey(body, key);
  if (at == (size_t)-1 || at >= body.length() || body[at] != '"') return false;
  ++at;
  String value;
  while (at < body.length() && body[at] != '"') {
    if (body[at] == '\\' && at + 1 < body.length()) ++at;  // Skip escapes.
    value += body[at];
    ++at;
  }
  out = value;
  return true;
}
