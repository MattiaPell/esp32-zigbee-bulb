#pragma once

// ---------------------------------------------------------------------------
// auto_update: checks GitHub for a newer release and installs it.
//
// Flow: once a day (and on demand) GET /OWNER/REPO/releases/latest, read the
// redirect to learn the newest tag (no API, no JSON). Installing downloads
// <tag>/esp32-zigbee-bulb.bin over HTTPS (certificate verified against the
// embedded CA bundle), checks it against <tag>/esp32-zigbee-bulb.bin.sha256,
// and only then switches the boot partition through the normal OTA path, so
// the bootloader rollback still protects against a firmware that does not
// start. Network work runs in a short-lived FreeRTOS task.
//
// Modes (persisted in NVS): off (default), notify (show it, you install) and
// auto (install by itself). A tag that was installed but rolled back is not
// retried automatically. Not available in MQTT builds (the generic release
// image is built without MQTT, so installing it would silently drop the
// bridge); there the status reports "supported":false.
// ---------------------------------------------------------------------------

#include <Arduino.h>

void updateBegin();

// Schedules the daily check and, in auto mode, the installation.
void updateTick();

bool updateSupported();

// "off", "notify" or "auto". False if unknown or updates are unsupported.
bool updateSetMode(const String &name);

// Starts a check now. False if a job is running, Wi-Fi is down or the clock
// is not synchronized yet (HTTPS needs a valid date).
bool updateCheckNow();

// Installs the newest known release. False if none is newer or a job runs.
bool updateInstallNow();

// Latest known newer version ("" if none), for the status line.
String updateAvailableVersion();

// {"supported":true,"mode":"notify","current":"1.1.0","latest":"1.2.0",
//  "available":true,"state":"idle","progress":0,"error":"",
//  "last_check_s":42,"auto_blocked":false}
String updateStatusJson();
