#pragma once

// ---------------------------------------------------------------------------
// credentials: Wi-Fi SSID/password and the API token, from secrets.h or from
// NVS. A personally built firmware (real secrets.h) seeds NVS once; generic
// release images (placeholder secrets) then keep working across updates.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Resolves the values and refreshes the stored copy. Call once, early.
void credBegin();

const String &credWifiSsid();      // "" when Wi-Fi was never configured
const String &credWifiPassword();
const String &credOtaToken();      // "" or placeholder => token auth disabled
