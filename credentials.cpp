#include "credentials.h"

#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#include "credentials_logic.h"
#include "debug_log.h"

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef OTA_TOKEN
#define OTA_TOKEN ""
#endif

namespace {

constexpr char CRED_NAMESPACE[] = "cred";

String ssid;
String pass;
String token;

}  // namespace

void credBegin() {
  Preferences prefs;
  prefs.begin(CRED_NAMESPACE, false);
  const String sSsid = prefs.getString("ssid", "");
  const String sPass = prefs.getString("pass", "");
  const String sToken = prefs.getString("otatok", "");

  if (credWifiNeedsSeed(WIFI_SSID, WIFI_PASSWORD, sSsid, sPass)) {
    prefs.putString("ssid", WIFI_SSID);
    prefs.putString("pass", WIFI_PASSWORD);
    debugLogPrintln("Credentials: Wi-Fi settings stored for release updates");
  }
  if (credTokenNeedsSeed(OTA_TOKEN, sToken)) {
    prefs.putString("otatok", OTA_TOKEN);
  }
  prefs.end();

  const WifiCreds wifi = credResolveWifi(WIFI_SSID, WIFI_PASSWORD, sSsid, sPass);
  ssid = wifi.ssid;
  pass = wifi.pass;
  token = credResolveToken(OTA_TOKEN, sToken);
  if (ssid.length() == 0) {
    debugLogPrintln("Credentials: no Wi-Fi configured (flash a build with secrets.h once)");
  }
  if (!credTokenUsable(token)) {
    debugLogPrintln("Credentials: no API token configured; token-protected routes are disabled");
  }
}

const String &credWifiSsid() { return ssid; }
const String &credWifiPassword() { return pass; }
const String &credOtaToken() { return token; }
