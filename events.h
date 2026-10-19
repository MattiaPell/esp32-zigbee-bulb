#pragma once

// ---------------------------------------------------------------------------
// events: Server-Sent Events change stream on its own port (EVENTS_PORT).
//
// The core WebServer is single-threaded, so a long-lived stream cannot share
// it: this module runs a tiny second listener (WiFiServer, core-bundled) and
// keeps up to EVENTS_MAX_CLIENTS streams open. It publishes only a change
// counter; the web page reacts by fetching the REST API once instead of
// polling every 2.5 s. Runtime-only.
// ---------------------------------------------------------------------------

#include <Arduino.h>

void eventsBegin();

// Accepts clients, detects state changes, pushes events and heartbeats.
void eventsTick();

size_t eventsClientCount();
