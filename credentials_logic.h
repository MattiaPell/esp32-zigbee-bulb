#pragma once

// ---------------------------------------------------------------------------
// credentials_logic: which Wi-Fi / token value wins (header-only, testable).
//
// Values in secrets.h are compiled in. A generic release image is built from
// secrets.example.h, so its values are placeholders and must never be used:
// the device falls back to what an earlier, personally built firmware stored
// in NVS ("seeded" at boot). A real compiled value always wins and refreshes
// the stored copy, so changing secrets.h and reflashing behaves as before.
// ---------------------------------------------------------------------------

#include <Arduino.h>

inline bool credIsPlaceholder(const char *v) {
  return v == nullptr || v[0] == '\0' || strcmp(v, "your-wifi-name") == 0 ||
         strcmp(v, "your-wifi-password") == 0 || strcmp(v, "your-ota-secret") == 0;
}

struct WifiCreds {
  String ssid;
  String pass;
};

// A real compiled SSID takes its password with it (open networks have an
// empty one); otherwise both come from NVS.
inline WifiCreds credResolveWifi(const char *cSsid, const char *cPass,
                                 const String &sSsid, const String &sPass) {
  WifiCreds c;
  if (!credIsPlaceholder(cSsid)) {
    c.ssid = cSsid;
    c.pass = cPass != nullptr ? cPass : "";
  } else {
    c.ssid = sSsid;
    c.pass = sPass;
  }
  return c;
}

inline bool credWifiNeedsSeed(const char *cSsid, const char *cPass,
                              const String &sSsid, const String &sPass) {
  if (credIsPlaceholder(cSsid)) return false;
  return sSsid != cSsid || sPass != (cPass != nullptr ? cPass : "");
}

inline String credResolveToken(const char *compiled, const String &stored) {
  return credIsPlaceholder(compiled) ? stored : String(compiled);
}

inline bool credTokenNeedsSeed(const char *compiled, const String &stored) {
  return !credIsPlaceholder(compiled) && stored != compiled;
}

// A missing or placeholder token disables token authorization entirely: the
// public placeholder must never open the OTA / settings endpoints.
inline bool credTokenUsable(const String &token) {
  return token.length() > 0 && !credIsPlaceholder(token.c_str());
}
