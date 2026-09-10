#pragma once

// ---------------------------------------------------------------------------
// web_hooks: HTTP POST event notifications to external URLs.
//
// Up to WEBHOOK_MAX_URLS endpoints receive a JSON body per event:
//   {"event":"bulb_offline","bulb":"a4c1...","name":"Salotto",
//    "detail":"","uptime_s":123,"version":"1.0.0"}
// Events: boot, bulb_joined, bulb_removed, bulb_online, bulb_offline,
// timer_expired, scene_applied, preset_applied, remote_pressed,
// remote_bind_result, ota_success, ota_failed, ota_rejected.
//
// Delivery is one POST per configured URL, queued and non-blocking at the
// loop level (each POST itself blocks up to ~2 s while the connection runs).
// Failures retry up to WEBHOOK_MAX_ATTEMPTS with a growing backoff, then the
// event moves on to the next URL and is eventually dropped. Nothing is sent
// while Wi-Fi is down (events wait, attempts do not burn).
//
// URLs are configured via /api/hooks (persisted in NVS). If none were ever
// configured, WEBHOOK_URL_1 from secrets.h seeds the list once.
// ---------------------------------------------------------------------------

#include <Arduino.h>

void webHooksBegin();

// Processes the delivery queue; call from the main loop.
void webHooksTick();

// Queue an event for every configured URL. bulbId/bulbName/detail may be ""
// (bulbId = IEEE hex, detail = free text, e.g. the scene name). Returns
// false if the queue is full.
bool webHookEvent(const char *event, const char *bulbId, const char *bulbName,
                  const char *detail = "");

// Queue a test event to the given URL only (not saved).
bool webHookTest(const String &url);

// --- URL configuration (persisted, mirrored by the REST API) ---------------

size_t webHookUrlCount();
String webHookUrlAt(size_t index);  // "" if out of range

// Validates (http:// or https://, length limit) and persists. False if
// invalid or storage full.
bool webHookUrlAdd(const String &url);

// Removes a stored URL. False if unknown.
bool webHookUrlRemove(const String &url);

bool isValidWebHookUrl(const String &url);
