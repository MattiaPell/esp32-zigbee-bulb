#pragma once

// ---------------------------------------------------------------------------
// ota_update: firmware upload over HTTP (/api/ota) with bootloader rollback.
//
// The web layer streams the multipart upload into this module; the actual
// WebServer callbacks live in web_server.cpp. A successful upload switches
// the boot partition to the freshly written slot and reboots: from then on
// the image is "pending verify" and the bootloader rolls back to the old
// slot unless this firmware confirms itself after OTA_VALIDATION_MS without
// restarting.
//
// Optional request headers (must be collected by WebServer):
//   X-OTA-MD5    32 hex chars; upload is rejected on checksum mismatch.
//   X-OTA-TOKEN  required only if OTA_TOKEN is defined in secrets.h.
// ---------------------------------------------------------------------------

#include <Arduino.h>

// Drives the boot-validation state (pending verify detection). Call once
// at boot. The web layer paints the status LED via otaUploading().
void otaBegin();

// Confirms the running image after the grace period and reboots on schedule
// after a successful upload. Call from the main loop.
void otaTick();

// True while a firmware upload is streaming in (the main loop paints the
// status LED red and skips the other ticks).
bool otaUploading();

// True while the running image has not been confirmed yet (rollback armed).
bool otaPendingVerify();

// Label of the running app partition ("app0" / "app1"), "" if unknown.
const char *otaRunningSlot();

// --- Upload state machine (driven by the web layer) -------------------------

enum class OtaResult : uint8_t {
  Idle = 0,        // No upload since boot.
  Uploading,       // Data is streaming in.
  Done,            // Written and verified; reboot scheduled.
  Rejected,        // Token check failed (no data written).
  BeginFailed,     // No free slot / partition error.
  WriteFailed,     // Write error (image too big or flash error).
  FinishFailed,    // end() failed: corrupt image or MD5 mismatch.
  Aborted,         // Client disconnected mid-upload.
};

OtaResult otaResult();
size_t otaBytesWritten();

// Start a new upload. md5Hex may be empty (no checksum check).
void otaUploadStart(const String &md5Hex);

// Marks the current upload as rejected before any data is written.
void otaUploadReject();

void otaUploadWrite(uint8_t *data, size_t len);

// Finalizes the image and schedules the reboot (OTA_REBOOT_DELAY_MS).
void otaUploadEnd();

// Client went away mid-upload: discard everything.
void otaUploadAbort();
