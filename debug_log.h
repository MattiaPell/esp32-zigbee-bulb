#pragma once

#include <Arduino.h>

#include <stddef.h>
#include <stdint.h>

// In-RAM debug log: every firmware message goes both to Serial and to a small
// circular buffer readable via GET /api/logs (tab "Log" in the web UI).
// Volatile by design (RAM only, lost on reboot) — never touches NVS.

constexpr size_t DEBUG_LOG_LINES = 100;
constexpr size_t DEBUG_LOG_LINE_LEN = 128;  // Includes the NUL terminator.

// Empties the buffer and writes a boot marker line.
void debugLogBegin();

// printf-style: writes to Serial AND to the ring buffer. Same signature as
// Serial.printf, so call sites swap 1:1. A trailing newline in fmt ends the
// line; the stored copy drops it.
void debugLogPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// println-style convenience wrapper (used by Serial.println call sites).
void debugLogPrintln(const char *text);
void debugLogPrintln(const String &text);

struct DebugLogLine {
  uint32_t index;  // Monotonic, never reused; 0 is reserved (no line yet).
  uint32_t ms;     // millis() when the line was stored.
  char text[DEBUG_LOG_LINE_LEN];
};

// Copies up to maxLines entries with index > since (oldest first) into out.
// *first receives the oldest index still buffered so clients can detect that
// older lines have been overwritten by newer ones.
size_t debugLogSince(uint32_t since, DebugLogLine *out, size_t maxLines,
                     uint32_t *first);

// Drops every line; the index counter keeps advancing so concurrent viewers
// with a stale `since` simply wait for new lines.
void debugLogClear();
