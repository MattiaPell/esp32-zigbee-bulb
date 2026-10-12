// Tests for debug_log.cpp

#include <cstdint>
#include <string.h>

#include "check.h"

// Mock FreeRTOS / ESP32 portMUX
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) (void)m
#define portEXIT_CRITICAL(m) (void)m

// Mock Serial.print
struct MockSerial {
    void print(const char * /*text*/) {}
} Serial;

// Mock millis
#define millis millis_debug_log
uint32_t mock_millis_debug_log = 0;
uint32_t millis_debug_log() { return mock_millis_debug_log; }

#define debugLogPrintf debugLogPrintf_debug_log
#define debugLogPrintln debugLogPrintln_debug_log
#define debugLogClear debugLogClear_debug_log
#define debugLogSince debugLogSince_debug_log
#define debugLogBegin debugLogBegin_debug_log

#include "../debug_log.cpp"

void runDebugLogTests() {
  check::begin("debug_log");

  debugLogClear();
  mock_millis_debug_log = 1000;

  // Test debugLogPrintf
  debugLogPrintf("Test line %d\n", 1);
  debugLogPrintf("Test line %d", 2);

  DebugLogLine lines[5];
  uint32_t first = 0;

  // Test debugLogSince fetching all lines
  size_t n = debugLogSince(0, lines, 5, &first);
  CHECK_EQ(n, (size_t)2);
  CHECK_EQ(first, (uint32_t)1);
  CHECK_EQ(strcmp(lines[0].text, "Test line 1"), 0); // newline should be stripped
  CHECK_EQ(strcmp(lines[1].text, "Test line 2"), 0);
  CHECK_EQ(lines[0].ms, (uint32_t)1000);

  // Test debugLogSince with 'since' parameter
  n = debugLogSince(lines[0].index, lines, 5, &first);
  CHECK_EQ(n, (size_t)1);
  CHECK_EQ(first, (uint32_t)1);
  CHECK_EQ(strcmp(lines[0].text, "Test line 2"), 0);

  // Test circular buffer overflow
  for (size_t i = 0; i < DEBUG_LOG_LINES + 5; i++) {
    debugLogPrintf("Overflow %zu", i);
  }

  n = debugLogSince(0, lines, 5, &first); // Read first 5 lines of currently valid lines
  CHECK_EQ(n, (size_t)5);
  // Total added: 2 + 105 = 107. 107 - 100 = 7 overwritten. First valid index is 8.
  CHECK_EQ(first, (uint32_t)8);

  // Test debugLogClear
  debugLogClear();
  n = debugLogSince(0, lines, 5, &first);
  CHECK_EQ(n, (size_t)0);
}
