#pragma once

// ---------------------------------------------------------------------------
// web_auth: pure request-authorization helpers (header-only, host-testable).
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Token comparison whose duration does not depend on where the strings first
// differ (only on their lengths).
inline bool tokenMatches(const String &given, const char *expected) {
  const size_t expLen = strlen(expected);
  const size_t givenLen = given.length();
  uint8_t diff = (uint8_t)(givenLen != expLen);
  for (size_t i = 0; i < expLen; ++i) {
    const char g = i < givenLen ? given[i] : 0;
    diff |= (uint8_t)(g ^ expected[i]);
  }
  return diff == 0;
}

// True when value is `prefix + authority` or `prefix + authority + ":port..."`.
inline bool authorityMatches(const String &value, const char *prefix,
                             const String &authority) {
  String exact = prefix;
  exact += authority;
  if (value == exact) return true;
  exact += ":";
  return value.startsWith(exact);
}

// Host / Origin allow-list: the mDNS name, the bare hostname, the device IP
// and localhost. `prefix` is "" for Host and "http://" for Origin.
inline bool originOrHostAllowed(const String &value, const char *prefix,
                                const String &hostname, const String &ip) {
  String local = hostname;
  local += ".local";
  return authorityMatches(value, prefix, local) ||
         authorityMatches(value, prefix, ip) ||
         authorityMatches(value, prefix, "localhost") ||
         authorityMatches(value, prefix, hostname);
}

// Request gate shared by every route.
//  - Host (when sent) must be one of ours: blocks DNS rebinding, even on GET.
//  - State-changing requests from a browser carry Origin, which must be ours
//    ("null" is refused): blocks CSRF.
//  - State-changing requests without Origin are not from a browser; they are
//    accepted only with a valid token (curl, scripts, Home Assistant).
inline bool requestAuthorized(bool isGetOrHead, bool isOptions,
                              const String &host, const String &origin,
                              bool tokenValid, const String &hostname,
                              const String &ip) {
  if (host.length() > 0 && !originOrHostAllowed(host, "", hostname, ip)) {
    return false;
  }
  if (isOptions || isGetOrHead) return true;
  if (origin.length() == 0) return tokenValid;
  if (origin == "null") return false;
  return originOrHostAllowed(origin, "http://", hostname, ip);
}
