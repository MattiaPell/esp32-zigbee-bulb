#include "check.h"

#include <cstdint>
#include <cmath>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Mocks
#define millis millis_status_led

uint32_t mock_millis_status_led = 0;
uint32_t millis() { return mock_millis_status_led; }

uint32_t mock_pixel_color = 0;
bool mock_pixel_show_called = false;
bool mock_pixel_begin_called = false;
bool mock_pixel_clear_called = false;
uint8_t mock_pixel_brightness = 0;

// Include the source file directly for testing
#include "../status_led.cpp"

void resetStatusLedTest() {
    mock_millis_status_led = 0;
    mock_pixel_color = 0;
    mock_pixel_show_called = false;
    mock_pixel_begin_called = false;
    mock_pixel_clear_called = false;
    mock_pixel_brightness = 0;
    statusLedSetMode(StatusLedMode::Off);
}

void testStatusLedBegin() {
    check::begin("statusLed: begin");
    resetStatusLedTest();

    statusLedBegin();

    CHECK(mock_pixel_begin_called);
    CHECK(mock_pixel_clear_called);
    CHECK(mock_pixel_show_called);
    CHECK_EQ(mock_pixel_brightness, STATUS_LED_BRIGHTNESS);
}

void testStatusLedMode() {
    check::begin("statusLed: mode getter/setter");
    resetStatusLedTest();

    CHECK(statusLedGetMode() == StatusLedMode::Off);

    statusLedSetMode(StatusLedMode::Ready);
    CHECK(statusLedGetMode() == StatusLedMode::Ready);

    statusLedSetMode(StatusLedMode::WifiConnecting);
    CHECK(statusLedGetMode() == StatusLedMode::WifiConnecting);
}

void testStatusLedTickOff() {
    check::begin("statusLed: tick Off");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::Off);
    mock_pixel_show_called = false;

    statusLedTick();

    CHECK(mock_pixel_show_called);
    CHECK_EQ(mock_pixel_color, (uint32_t)0);
}

void testStatusLedTickReady() {
    check::begin("statusLed: tick Ready");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::Ready);
    mock_pixel_show_called = false;

    statusLedTick();

    CHECK(mock_pixel_show_called);
    uint32_t expectedColor = ((uint32_t)0 << 16) | ((uint32_t)40 << 8) | 0;
    CHECK_EQ(mock_pixel_color, expectedColor);
}

void testStatusLedTickWifiConnecting() {
    check::begin("statusLed: tick WifiConnecting");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::WifiConnecting);

    // (now / 400) % 2 == 0 -> Off
    mock_millis_status_led = 0;
    statusLedTick();
    CHECK_EQ(mock_pixel_color, (uint32_t)0);

    // (now / 400) % 2 != 0 -> Blue
    mock_millis_status_led = 400;
    statusLedTick();
    uint32_t expectedBlue = ((uint32_t)0 << 16) | ((uint32_t)0 << 8) | 60;
    CHECK_EQ(mock_pixel_color, expectedBlue);
}

void testStatusLedTickZigbeeStarting() {
    check::begin("statusLed: tick ZigbeeStarting");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::ZigbeeStarting);

    // (now / 400) % 2 == 0 -> Off
    mock_millis_status_led = 0;
    statusLedTick();
    CHECK_EQ(mock_pixel_color, (uint32_t)0);

    // (now / 400) % 2 != 0 -> Purple
    mock_millis_status_led = 400;
    statusLedTick();
    uint32_t expectedPurple = ((uint32_t)45 << 16) | ((uint32_t)0 << 8) | 45;
    CHECK_EQ(mock_pixel_color, expectedPurple);
}

void testStatusLedTickError() {
    check::begin("statusLed: tick Error");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::Error);

    // (now / 120) % 2 == 0 -> Red
    mock_millis_status_led = 0;
    statusLedTick();
    uint32_t expectedRed = ((uint32_t)45 << 16) | ((uint32_t)0 << 8) | 0;
    CHECK_EQ(mock_pixel_color, expectedRed);

    // (now / 120) % 2 != 0 -> Off
    mock_millis_status_led = 120;
    statusLedTick();
    CHECK_EQ(mock_pixel_color, (uint32_t)0);
}

void testStatusLedTickPairing() {
    check::begin("statusLed: tick Pairing");
    resetStatusLedTest();

    statusLedSetMode(StatusLedMode::Pairing);

    mock_millis_status_led = 0; // phase = 0, sin(0) = 0
    statusLedTick();
    // v = 40.0f * (0.5f + 0.0f) = 20
    uint8_t v = 20;
    uint32_t expectedAmber = ((uint32_t)v << 16) | ((uint32_t)(v * 0.45f) << 8) | 0;
    CHECK_EQ(mock_pixel_color, expectedAmber);
}

void runStatusLedTests() {
    testStatusLedBegin();
    testStatusLedMode();
    testStatusLedTickOff();
    testStatusLedTickReady();
    testStatusLedTickWifiConnecting();
    testStatusLedTickZigbeeStarting();
    testStatusLedTickError();
    testStatusLedTickPairing();
}
