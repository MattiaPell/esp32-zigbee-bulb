#pragma once

// ---------------------------------------------------------------------------
// update_check: pure helpers for the GitHub release updater (header-only,
// host-testable): version compare, redirect parsing, checksum file parsing.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Parses "1.2.3" / "v1.2.3" (digits only, exactly three parts).
inline bool parseVersion(const String &in, int out[3]) {
  size_t i = 0;
  if (in.length() > 0 && (in[0] == 'v' || in[0] == 'V')) i = 1;
  for (int part = 0; part < 3; ++part) {
    if (i >= in.length()) return false;
    long value = 0;
    size_t digits = 0;
    while (i < in.length() && in[i] >= '0' && in[i] <= '9') {
      value = value * 10 + (in[i] - '0');
      if (value > 100000) return false;
      ++i;
      ++digits;
    }
    if (digits == 0) return false;
    out[part] = (int)value;
    if (part < 2) {
      if (i >= in.length() || in[i] != '.') return false;
      ++i;
    }
  }
  return i == in.length();
}

// True when `latest` is a valid version strictly greater than `current`.
inline bool versionNewer(const String &current, const String &latest) {
  int c[3], l[3];
  if (!parseVersion(current, c) || !parseVersion(latest, l)) return false;
  for (int i = 0; i < 3; ++i) {
    if (l[i] != c[i]) return l[i] > c[i];
  }
  return false;
}

inline bool sameVersion(const String &a, const String &b) {
  int x[3], y[3];
  if (!parseVersion(a, x) || !parseVersion(b, y)) return false;
  return x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
}

// Extracts the tag from the redirect GitHub answers to
// /OWNER/REPO/releases/latest: ".../OWNER/REPO/releases/tag/v1.2.3", absolute
// or relative. The path must belong to `repo` and the tag must be a plain
// version string, so a odd Location can never steer the download elsewhere.
inline bool tagFromLocation(const String &location, const char *repo, String &tagOut) {
  String loc = location;
  const int scheme = loc.indexOf("://");
  if (scheme >= 0) {
    if (!loc.startsWith("https://github.com/")) return false;
    loc = loc.substring(scheme + 3 + strlen("github.com"));  // "/OWNER/REPO/..."
  }
  String prefix = "/";
  prefix += repo;
  prefix += "/releases/tag/";
  String head = loc.substring(0, prefix.length() < loc.length() ? prefix.length() : loc.length());
  if (!head.equalsIgnoreCase(prefix)) return false;
  String tag = loc.substring(prefix.length());
  const int cut = tag.indexOf('?');
  if (cut >= 0) tag = tag.substring(0, cut);
  int v[3];
  if (!parseVersion(tag, v)) return false;
  tagOut = tag;
  return true;
}

inline String releaseAssetUrl(const char *repo, const String &tag, const char *asset) {
  String url = "https://github.com/";
  url += repo;
  url += "/releases/download/";
  url += tag;
  url += "/";
  url += asset;
  return url;
}

// First 64 hex digits of a "sha256sum" style file ("<hash>  <name>").
inline bool parseSha256Hex(const String &body, uint8_t out[32]) {
  size_t i = 0;
  while (i < body.length() && isspace((unsigned char)body[i])) ++i;
  if (body.length() < i + 64) return false;
  for (size_t n = 0; n < 32; ++n) {
    int v[2];
    for (int k = 0; k < 2; ++k) {
      const char c = body[i + n * 2 + k];
      if (c >= '0' && c <= '9') v[k] = c - '0';
      else if (c >= 'a' && c <= 'f') v[k] = c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') v[k] = c - 'A' + 10;
      else return false;
    }
    out[n] = (uint8_t)((v[0] << 4) | v[1]);
  }
  const size_t end = i + 64;
  return end == body.length() || isspace((unsigned char)body[end]);
}

inline bool sha256Equal(const uint8_t a[32], const uint8_t b[32]) {
  uint8_t diff = 0;
  for (int i = 0; i < 32; ++i) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}
