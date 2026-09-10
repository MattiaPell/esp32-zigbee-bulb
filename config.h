#pragma once

#include <Arduino.h>
#include <Preferences.h>

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
constexpr uint8_t MAX_REMOTES = 4;        // Zigbee remote/steering device cap
constexpr uint8_t REMOTE_DIM_STEP_PCT = 10;  // Per press (ring/step), 0-100
constexpr uint16_t ZIGBEE_GROUP_ID = 0x0001;  // The shared "all lights" group
constexpr bool USE_ZIGBEE_GROUPS = true;      // False = always per-bulb unicast

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

// Stored mDNS hostname (shared storage: namespace "bulbs", key "host";
// written by the web server and serial console).
inline String storedHostname() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true);
  const String host = prefs.getString("host", DEFAULT_HOSTNAME);
  prefs.end();
  return host;
}

// ---------------------------------------------------------------------------
// Firmware / OTA
// ---------------------------------------------------------------------------
constexpr char FW_VERSION[] = "1.1.0";
// After an OTA reboot the new slot stays "pending verify": the bootloader
// rolls back to the previous slot if the board restarts before this grace
// expires (survive the window = the image is confirmed valid).
constexpr uint32_t OTA_VALIDATION_MS = 90000;
// Delay before rebooting after a successful upload, so the HTTP response
// reaches the client first.
constexpr uint32_t OTA_REBOOT_DELAY_MS = 1500;

// ---------------------------------------------------------------------------
// Webhooks
// ---------------------------------------------------------------------------
constexpr size_t WEBHOOK_MAX_URLS = 3;
constexpr size_t WEBHOOK_MAX_URL_LENGTH = 120;
constexpr uint8_t WEBHOOK_MAX_ATTEMPTS = 3;      // Per URL, then move on
constexpr uint32_t WEBHOOK_CONNECT_TIMEOUT_MS = 1200;
constexpr uint32_t WEBHOOK_RESPONSE_TIMEOUT_MS = 2500;

// ---------------------------------------------------------------------------
// MQTT bridge (compiled in only when MQTT_HOST is defined in secrets.h)
// ---------------------------------------------------------------------------
constexpr uint16_t MQTT_KEEPALIVE_S = 60;         // Protocol keepalive
constexpr uint16_t MQTT_PING_EVERY_MS = 25000;    // PINGREQ period (in ms)
constexpr uint16_t MQTT_REPUBLISH_MS = 100;       // State push rate limit
constexpr uint32_t MQTT_CONNECT_BACKOFF_MS = 5000;  // First reconnect delay
constexpr uint32_t MQTT_RECONNECT_MAX_MS = 60000;   // Reconnect backoff cap
