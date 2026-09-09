#pragma once

// ---------------------------------------------------------------------------
// config_backup: export/import of the user-facing configuration as one JSON
// document (hostname, bulbs with names and states, scenes, webhook URLs,
// MQTT toggle). The format carries a "version" field; unknown fields are
// ignored by the restore.
//
// After a clean flash: restore FIRST, then re-pair the bulbs — when a bulb
// with a restored IEEE joins, it reappears already named with its state.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// The full configuration document (see README for the shape).
String configBackupJson();

// Applies a backup document. Unknown/oversized entries are skipped and
// counted. Returns false when the document is not a recognizable backup;
// on success `summary` holds the counts object for the API response.
bool configRestoreJson(const String &body, String &summary);
