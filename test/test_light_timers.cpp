#include "check.h"
#include <cstdint>

uint32_t mock_millis = 0;
uint32_t millis() { return mock_millis; }

#include "bulb_registry.h"
#include "debug_log.h"
#include "web_hooks.h"
#include "zigbee_bulbs.h"

// Tracking variables for assertions
int sendOffCalls = 0;
int sendAllOffCalls = 0;
int webhookCalls = 0;

// Stubs for dependencies
void bulbSendOff(Bulb * /*bulb*/) { sendOffCalls++; }
int bulbSendAllOff() { sendAllOffCalls++; return 1; }
bool webHookEvent(const char * /*event*/, const char * /*bulbId*/, const char * /*bulbName*/, const char * /*detail*/) {
  webhookCalls++;
  return true;
}
void debugLogPrintf(const char * /*format*/, ...) {}
void debugLogPrintln(const char * /*line*/) {}

// Simple mock registry
Bulb mockBulbs[3];
size_t mockBulbCount = 3;

Bulb* registryFindByIeee(const esp_zb_ieee_addr_t ieee) {
  for (size_t i = 0; i < mockBulbCount; ++i) {
    if (memcmp(mockBulbs[i].ieee, ieee, sizeof(esp_zb_ieee_addr_t)) == 0) {
      return &mockBulbs[i];
    }
  }
  return nullptr;
}

String bulbIeeeHex(const Bulb * /*bulb*/) {
  return "testhex";
}

// Compile the source directly to test its static functions
#include "../light_timers.cpp"

namespace {

void resetMocks() {
  mock_millis = 10000;  // Start away from 0
  sendOffCalls = 0;
  sendAllOffCalls = 0;
  webhookCalls = 0;
  for (size_t i = 0; i < SLOTS; ++i) {
    clearSlot(i);
  }

  // Initialize mock bulbs with distinct IEEE addresses
  for (int i = 0; i < 3; ++i) {
    mockBulbs[i] = Bulb();
    mockBulbs[i].ieee[0] = i + 1;
    snprintf(mockBulbs[i].name, sizeof(mockBulbs[i].name), "MockBulb%d", i);
  }
}

void testSingleBulbTimer() {
  check::begin("light timers: single bulb");
  resetMocks();

  Bulb* b = &mockBulbs[0];
  bulbTimerSet(b, 60);

  CHECK_EQ(bulbTimerRemaining(b), 60u);

  mock_millis += 30000;
  lightTimersTick();
  CHECK_EQ(bulbTimerRemaining(b), 30u);
  CHECK_EQ(sendOffCalls, 0);
  CHECK_EQ(webhookCalls, 0);

  mock_millis += 30000;
  lightTimersTick();
  CHECK_EQ(bulbTimerRemaining(b), 0u);
  CHECK_EQ(sendOffCalls, 1);
  CHECK_EQ(webhookCalls, 1);
}

void testAllBulbsTimer() {
  check::begin("light timers: all bulbs");
  resetMocks();

  bulbTimerSetAll(120);
  CHECK_EQ(bulbTimerRemainingAll(), 120u);

  mock_millis += 60000;
  lightTimersTick();
  CHECK_EQ(bulbTimerRemainingAll(), 60u);
  CHECK_EQ(sendAllOffCalls, 0);
  CHECK_EQ(webhookCalls, 0);

  mock_millis += 60000;
  lightTimersTick();
  CHECK_EQ(bulbTimerRemainingAll(), 0u);
  CHECK_EQ(sendAllOffCalls, 1);
  CHECK_EQ(webhookCalls, 1);
}

void testTimerCancellation() {
  check::begin("light timers: cancellation");
  resetMocks();

  Bulb* b = &mockBulbs[1];
  bulbTimerSet(b, 300);
  CHECK_EQ(bulbTimerRemaining(b), 300u);

  bulbTimerSet(b, 0); // 0 cancels the timer
  CHECK_EQ(bulbTimerRemaining(b), 0u);

  mock_millis += 300000;
  lightTimersTick();
  CHECK_EQ(sendOffCalls, 0); // Timer was cancelled, no off call
}

void testGlobalTimerCancellation() {
  check::begin("light timers: global cancellation");
  resetMocks();

  bulbTimerSetAll(600);
  CHECK_EQ(bulbTimerRemainingAll(), 600u);

  bulbTimerSetAll(0);
  CHECK_EQ(bulbTimerRemainingAll(), 0u);

  mock_millis += 600000;
  lightTimersTick();
  CHECK_EQ(sendAllOffCalls, 0);
}

void testMultipleIndependentTimers() {
  check::begin("light timers: multiple timers");
  resetMocks();

  Bulb* b1 = &mockBulbs[0];
  Bulb* b2 = &mockBulbs[1];

  bulbTimerSet(b1, 10);
  bulbTimerSet(b2, 20);

  mock_millis += 10000;
  lightTimersTick();
  CHECK_EQ(sendOffCalls, 1); // b1 expires
  CHECK_EQ(bulbTimerRemaining(b2), 10u);

  mock_millis += 10000;
  lightTimersTick();
  CHECK_EQ(sendOffCalls, 2); // b2 expires
}

void testTimerUpdate() {
  check::begin("light timers: update timer");
  resetMocks();

  Bulb* b = &mockBulbs[0];
  bulbTimerSet(b, 60);
  mock_millis += 30000;
  CHECK_EQ(bulbTimerRemaining(b), 30u);

  bulbTimerSet(b, 120); // Update existing timer
  CHECK_EQ(bulbTimerRemaining(b), 120u);

  mock_millis += 60000;
  lightTimersTick();
  CHECK_EQ(sendOffCalls, 0); // Not expired yet

  mock_millis += 60000;
  lightTimersTick();
  CHECK_EQ(sendOffCalls, 1); // Now it expires
}

void testTimerLimit() {
  check::begin("light timers: max limit");
  resetMocks();

  Bulb* b = &mockBulbs[0];
  // Set above TIMER_LIMIT_S (which is 24 * 60 * 60 = 86400)
  bulbTimerSet(b, 100000);
  CHECK_EQ(bulbTimerRemaining(b), 86400u);
}

void testUnknownBulbIgnored() {
  check::begin("light timers: unknown bulb ignored");
  resetMocks();

  Bulb b;
  b.ieee[0] = 99; // Unknown IEEE
  bulbTimerSet(&b, 10);

  mock_millis += 10000;
  lightTimersTick();

  // Timer expires, but registryFindByIeee returns nullptr, so no off call
  CHECK_EQ(sendOffCalls, 0);
  // It shouldn't trigger webhook either
  CHECK_EQ(webhookCalls, 0);
}

} // namespace

void runLightTimerTests() {
  testSingleBulbTimer();
  testAllBulbsTimer();
  testTimerCancellation();
  testGlobalTimerCancellation();
  testMultipleIndependentTimers();
  testTimerUpdate();
  testTimerLimit();
  testUnknownBulbIgnored();
}
