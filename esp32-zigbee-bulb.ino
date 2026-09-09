// esp32-zigbee-bulb — Zigbee coordinator + web controller for IKEA bulbs.
//
// One ESP32-C6 forms the Zigbee network, pairs any number of IKEA bulbs and
// serves a control page over Wi-Fi (multi-bulb, per-bulb state, restore
// after power cut, BOOT button kill switch).
//
// Arduino IDE settings (see README):
//   Board:              ESP32C6 Dev Module
//   USB CDC On Boot:    Enabled
//   Zigbee mode:        Zigbee ZCZR (coordinator/router)
//   Partition Scheme:   Custom (uses partitions.csv from this sketch)
//
// License: MIT. See README for credits.

#include <Arduino.h>
#include <WiFi.h>
#include <Zigbee.h>

#ifndef ZIGBEE_MODE_ZCZR
#error "Select Tools > Zigbee mode > Zigbee ZCZR (coordinator/router)"
#endif

#include "bulb_registry.h"
#include "config.h"
#include "light_timers.h"
#include "mqtt_bridge.h"
#include "ota_update.h"
#include "power_button.h"
#include "remote_controls.h"
#include "scenes.h"
#include "serial_console.h"
#include "status_led.h"
#include "web_hooks.h"
#include "web_server.h"
#include "zigbee_bulbs.h"

namespace {

// BOOT button kill switch: every bulb off in one press.
void onKillSwitch() {
  bulbSendAllOff();
}

// Drives the onboard LED from the current system state.
void updateStatusLed() {
  if (otaUploading()) {
    statusLedSetMode(StatusLedMode::Error);  // Fast red during firmware upload.
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    statusLedSetMode(StatusLedMode::WifiConnecting);
    return;
  }
  if (!Zigbee.started() || !Zigbee.connected()) {
    statusLedSetMode(StatusLedMode::ZigbeeStarting);
    return;
  }
  if (zigbeePairingActive() || registryCount() == 0) {
    statusLedSetMode(StatusLedMode::Pairing);
    return;
  }
  statusLedSetMode(StatusLedMode::Ready);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  serialConsoleBegin();

  statusLedBegin();
  statusLedSetMode(StatusLedMode::WifiConnecting);

  registryBegin();
  scenesBegin();
  lightTimersBegin();
  remotesBegin();  // Registered Zigbee remotes/steering devices.
  powerButtonBegin(onKillSwitch);

  webBegin();
  webHooksBegin();  // Loads webhook URLs; a "boot" event announces startup.
  webHookEvent("boot", "", "", "");
  mqttBegin();  // No-op unless MQTT_HOST is defined in secrets.h.

  statusLedSetMode(StatusLedMode::ZigbeeStarting);
  zigbeeBegin();  // Restarts the board if the stack cannot start.

  otaBegin();  // Arms the pending-verify confirmation window if just OTA'd.

  Serial.println("Ready. Web UI: see the IP printed above, or bulb.local");
}

void loop() {
  // While a firmware upload streams in, keep the loop minimal: flash erase/
  // write gets exclusive access and the other ticks resume after the reboot.
  if (otaUploading()) {
    otaTick();
    updateStatusLed();
    statusLedTick();
    delay(2);
    return;
  }

  webTick();
  serialConsoleTick();
  powerButtonTick();
  zigbeeTick();
  lightTimersTick();
  registryTick();
  otaTick();
  webHooksTick();
  mqttTick();

  updateStatusLed();
  statusLedTick();

  delay(2);
}
