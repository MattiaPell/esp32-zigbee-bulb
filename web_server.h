#pragma once

// Web server, REST API, Wi-Fi connection and mDNS registration.
// All routes are dispatched from one handler (WebServer has no path
// parameters), see web_server.cpp.

#include <Arduino.h>

// Connects Wi-Fi (blocking up to WIFI_CONNECT_TIMEOUT_MS), registers routes
// and starts the server. mDNS registers as soon as Wi-Fi is up.
void webBegin();

// handleClient + Wi-Fi retry + (re)registration of mDNS.
void webTick();

// Validates, persists and applies a new mDNS hostname. False if invalid.
// Used by the REST API and the serial console.
bool webSetHostname(const String &name);
