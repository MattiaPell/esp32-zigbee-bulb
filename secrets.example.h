#pragma once

// Copy this file to secrets.h and enter your 2.4 GHz Wi-Fi credentials.
// secrets.h is ignored by Git and must not be committed.
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// Optional: require this token in the X-OTA-TOKEN header for firmware
// uploads (POST /api/ota). Uncomment to enable.
// #define OTA_TOKEN "your-ota-secret"

// Optional: seed one webhook URL for event notifications (used only if no
// URL was ever configured via /api/hooks). Uncomment to enable.
// #define WEBHOOK_URL_1 "https://example.com/light-hooks"

// ---------------------------------------------------------------------
// MQTT bridge (compiled in only when MQTT_HOST is defined).
// Plain TCP (port 1883); TLS is not supported by the minimal client.
// MQTT_USER / MQTT_PASS are optional (anonymous brokers need neither).
// ---------------------------------------------------------------------
// #define MQTT_HOST "192.168.1.10"
// #define MQTT_PORT 1883
// #define MQTT_USER "homeassistant"
// #define MQTT_PASS "your-mqtt-password"
// #define MQTT_PREFIX "bulbctl"     // Topic prefix (default "bulbctl")
// #define MQTT_DISCOVERY 1          // 0 = no Home Assistant discovery
