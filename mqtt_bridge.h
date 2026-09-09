#pragma once

// ---------------------------------------------------------------------------
// mqtt_bridge: minimal MQTT 3.1.1 client for Home Assistant integration.
//
// Hand-rolled over WiFiClient (the ESP32 Arduino core ships no MQTT client
// and the project takes no third-party libraries). Everything is QoS 0 and
// non-blocking at the loop level (each reconnect attempt may block for the
// TCP connect timeout; retries back off).
//
// Compiled in ONLY when MQTT_HOST is defined in secrets.h; a runtime toggle
// (/api/mqtt, persisted) can turn the bridge off without reflashing.
//
// Topic layout (prefix configurable, default "bulbctl"; id = IEEE hex):
//   <prefix>/bridge               retained online/offline, LWT of the bridge
//   <prefix>/<ieee>/state         retained bulb state
//                                 {"state":"ON","brightness":128,
//                                  "color_mode":"color_temp"|"rgb",
//                                  "color_temp":370,"color":{"r":..,"g":..,"b":..}}
//   <prefix>/<ieee>/availability  retained online/offline per bulb
//   <prefix>/<ieee>/set           commands (same fields as the REST PATCH)
//   <prefix>/all/set              same, applied to every bulb
//   homeassistant/light/<prefix>-<hostname>-<ieee>/light/config
//                                 retained HA discovery (MQTT_DISCOVERY=1)
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Optional settings (all overridable in secrets.h next to MQTT_HOST).
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_PREFIX
#define MQTT_PREFIX "bulbctl"
#endif
#ifndef MQTT_DISCOVERY
#define MQTT_DISCOVERY 1
#endif

void mqttBegin();

// State machine + traffic; call from the main loop.
void mqttTick();

bool mqttConnected();

// Runtime toggle (persisted in NVS); turning it off drops the connection.
bool mqttEnabledRuntime();
void mqttSetEnabled(bool enabled);
