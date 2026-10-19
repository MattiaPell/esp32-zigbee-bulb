#pragma once

// ---------------------------------------------------------------------------
// Display-name sanitizer shared by bulbs and remotes (header-only, testable).
// Keeps printable ASCII and well-formed UTF-8 (so "Cucina è" survives);
// replaces control characters, the characters that would break JSON or the
// NVS blob separators (" \ | ;) and malformed bytes with a space. Never cuts
// a multi-byte character in half when the buffer is full.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Length of a well-formed UTF-8 sequence starting at p (0 if malformed).
inline size_t utf8SeqLen(const unsigned char *p) {
  const unsigned char c = p[0];
  size_t n;
  if (c >= 0xC2 && c <= 0xDF) n = 2;
  else if (c >= 0xE0 && c <= 0xEF) n = 3;
  else if (c >= 0xF0 && c <= 0xF4) n = 4;
  else return 0;
  for (size_t i = 1; i < n; ++i) {
    if ((p[i] & 0xC0) != 0x80) return 0;  // Also stops at the NUL terminator.
  }
  if (c == 0xE0 && p[1] < 0xA0) return 0;  // Overlong.
  if (c == 0xED && p[1] > 0x9F) return 0;  // UTF-16 surrogates.
  if (c == 0xF0 && p[1] < 0x90) return 0;  // Overlong.
  if (c == 0xF4 && p[1] > 0x8F) return 0;  // Beyond U+10FFFF.
  return n;
}

inline void sanitizeDisplayName(char *out, size_t cap, const char *in,
                                const char *fallback) {
  size_t o = 0;
  const unsigned char *p = (const unsigned char *)in;
  while (*p != '\0' && o + 1 < cap) {
    const unsigned char c = *p;
    if (c >= 0x80) {
      const size_t n = utf8SeqLen(p);
      if (n > 0) {
        if (o + n + 1 > cap) break;  // Would not fit whole.
        for (size_t i = 0; i < n; ++i) out[o++] = (char)p[i];
        p += n;
        continue;
      }
      out[o++] = ' ';
    } else if (c == '"' || c == '\\' || c == '|' || c == ';' || c < 0x20 ||
               c == 0x7f) {
      out[o++] = ' ';
    } else {
      out[o++] = (char)c;
    }
    ++p;
  }
  while (o > 0 && out[o - 1] == ' ') o--;
  out[o] = '\0';
  if (out[0] == '\0') snprintf(out, cap, "%s", fallback);
}
