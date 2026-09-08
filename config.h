#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Hardware pins (ESP32-C6 DevKitM-1). Change here for other boards.
// ---------------------------------------------------------------------------
constexpr uint8_t STATUS_PIXEL_PIN = 8;  // Onboard NeoPixel RGB status LED
constexpr uint8_t POWER_BUTTON_PIN = 9;  // BOOT button: kill switch (all off)
constexpr uint8_t STATUS_LED_BRIGHTNESS = 40;  // 0-255, keep it comfortable

// ---------------------------------------------------------------------------
// Zigbee
// ---------------------------------------------------------------------------
constexpr uint8_t BULB_ENDPOINT = 5;      // Controller endpoint used for binding
constexpr uint8_t PAIRING_SECONDS = 180;  // Network open window for new bulbs
constexpr uint8_t MAX_BULBS = 16;         // Registry cap

// ---------------------------------------------------------------------------
// Wi-Fi
// ---------------------------------------------------------------------------
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t WIFI_RETRY_MS = 30000;

// ---------------------------------------------------------------------------
// Lights
// ---------------------------------------------------------------------------
constexpr int MIN_KELVIN = 2200;
constexpr int MAX_KELVIN = 4000;
constexpr int DEFAULT_KELVIN = 3500;
constexpr uint8_t DEFAULT_BRIGHTNESS_PCT = 50;  // Shown and stored as 0-100 %
constexpr uint16_t DEFAULT_TRANSITION_DS = 5;   // Zigbee transition, 0.1 s units
constexpr uint16_t MAX_TRANSITION_DS = 600;     // 60 s cap

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------
constexpr char PREFS_NAMESPACE[] = "bulbs";
constexpr uint32_t STATE_SAVE_DELAY_MS = 2000;  // Debounce for NVS writes

// ---------------------------------------------------------------------------
// Web
// ---------------------------------------------------------------------------
constexpr char DEFAULT_HOSTNAME[] = "bulb";
constexpr size_t MAX_HOSTNAME_LENGTH = 31;  // DNS label limit
constexpr uint16_t WEB_PORT = 80;
