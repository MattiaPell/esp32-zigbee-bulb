#include "debug_log.h"

#include <stdarg.h>

#include <WString.h>

namespace {

DebugLogLine lines[DEBUG_LOG_LINES];
size_t head = 0;         // Next slot to write.
size_t count = 0;        // Valid lines currently buffered (<= DEBUG_LOG_LINES).
uint32_t nextIndex = 1;  // Index assigned to the next line; never reused.

// Written from both the loop task and Zigbee stack callbacks.
portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

void storeLine(const char *text, uint32_t ms) {
  portENTER_CRITICAL(&logMux);
  DebugLogLine &slot = lines[head];
  slot.index = nextIndex++;
  slot.ms = ms;
  strlcpy(slot.text, text, sizeof(slot.text));
  head = (head + 1) % DEBUG_LOG_LINES;
  if (count < DEBUG_LOG_LINES) ++count;
  portEXIT_CRITICAL(&logMux);
}

}  // namespace

void debugLogBegin() {
  // Buffer is static; nothing to init. Marker line so the web UI shows where
  // this boot starts.
  debugLogPrintf("-- boot --\n");
}

void debugLogPrintf(const char *fmt, ...) {
  char text[DEBUG_LOG_LINE_LEN];
  va_list args;
  va_start(args, fmt);
  vsnprintf(text, sizeof(text), fmt, args);
  va_end(args);

  Serial.print(text);  // Serial output keeps the exact original behaviour.

  // Store without the trailing newline: lines are JSON strings later.
  const size_t n = strlen(text);
  if (n > 0 && text[n - 1] == '\n') text[n - 1] = '\0';
  storeLine(text, millis());
}

void debugLogPrintln(const char *text) {
  debugLogPrintf("%s\n", text);
}

void debugLogPrintln(const String &text) {
  debugLogPrintln(text.c_str());
}

size_t debugLogSince(uint32_t since, DebugLogLine *out, size_t maxLines,
                     uint32_t *first) {
  size_t n = 0;
  portENTER_CRITICAL(&logMux);
  *first = count == 0 ? nextIndex : lines[(head + DEBUG_LOG_LINES - count) %
                                          DEBUG_LOG_LINES].index;
  for (size_t i = 0; i < count && n < maxLines; ++i) {
    const DebugLogLine &src =
        lines[(head + DEBUG_LOG_LINES - count + i) % DEBUG_LOG_LINES];
    if (src.index <= since) continue;
    out[n++] = src;
  }
  portEXIT_CRITICAL(&logMux);
  return n;
}

void debugLogClear() {
  portENTER_CRITICAL(&logMux);
  head = 0;
  count = 0;  // nextIndex keeps advancing: viewers with a stale `since` wait.
  portEXIT_CRITICAL(&logMux);
}
