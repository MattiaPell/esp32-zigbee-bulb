#include "ota_update.h"

#include <Update.h>
#include <esp_ota_ops.h>

#include "bulb_registry.h"
#include "config.h"
#include "debug_log.h"
#include "web_hooks.h"

namespace {

OtaResult result = OtaResult::Idle;
size_t bytesWritten = 0;
uint32_t rebootAtMs = 0;

// Boot validation of a freshly OTA'd slot (pending verify -> valid).
bool validating = false;
uint32_t validatingSinceMs = 0;

const esp_partition_t *runningPartition() {
  return esp_ota_get_running_partition();
}

}  // namespace

void otaBegin() {
  const esp_partition_t *running = runningPartition();
  esp_ota_img_states_t state;
  if (running != nullptr &&
      esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    validating = true;
    validatingSinceMs = millis();
    debugLogPrintf("OTA: slot %s pending verify; confirming after %u s if stable\n",
                  running->label, (unsigned)(OTA_VALIDATION_MS / 1000));
  }
}

void otaTick() {
  if (validating && millis() - validatingSinceMs >= OTA_VALIDATION_MS) {
    validating = false;
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    debugLogPrintf("OTA: image confirmed valid (%s)\n", esp_err_to_name(err));
  }
  if (result == OtaResult::Done && rebootAtMs != 0 &&
      (int32_t)(millis() - rebootAtMs) >= 0) {
    debugLogPrintln("OTA: rebooting into the new firmware...");
    delay(100);
    ESP.restart();
  }
}

bool otaUploading() {
  return result == OtaResult::Uploading;
}

bool otaPendingVerify() {
  const esp_partition_t *running = runningPartition();
  esp_ota_img_states_t state;
  return running != nullptr &&
         esp_ota_get_state_partition(running, &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

const char *otaRunningSlot() {
  const esp_partition_t *running = runningPartition();
  return running != nullptr ? running->label : "";
}

OtaResult otaResult() {
  return result;
}

size_t otaBytesWritten() {
  return bytesWritten;
}

void otaUploadStart(const String &md5Hex) {
  if (result == OtaResult::Uploading) return;  // Only the first file part counts.

  registryFlush();  // No pending NVS write during flash erase/write.
  bytesWritten = 0;
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    result = OtaResult::BeginFailed;
    debugLogPrintf("OTA: begin failed: %s\n", Update.errorString());
    char detail[48];
    snprintf(detail, sizeof(detail), "begin: %s", Update.errorString());
    webHookEvent("ota_failed", "", "", detail);
    return;
  }
  if (md5Hex.length() == 32) {
    Update.setMD5(md5Hex.c_str());
    debugLogPrintf("OTA: MD5 check enabled (%s)\n", md5Hex.c_str());
  }
  result = OtaResult::Uploading;
  debugLogPrintln("OTA: upload started");
}

void otaUploadReject() {
  result = OtaResult::Rejected;
  webHookEvent("ota_rejected", "", "", "token");
}

void otaUploadWrite(uint8_t *data, size_t len) {
  if (result != OtaResult::Uploading) return;
  const size_t written = Update.write(data, len);
  if (written != len) {
    result = OtaResult::WriteFailed;
    Update.abort();
    debugLogPrintf("OTA: write failed after %u bytes: %s\n",
                  (unsigned)bytesWritten, Update.errorString());
    char detail[48];
    snprintf(detail, sizeof(detail), "write: %s", Update.errorString());
    webHookEvent("ota_failed", "", "", detail);
    return;
  }
  bytesWritten += written;
}

void otaUploadEnd() {
  if (result != OtaResult::Uploading) return;
  if (!Update.end(true)) {
    result = OtaResult::FinishFailed;
    debugLogPrintf("OTA: finish failed after %u bytes: %s\n",
                  (unsigned)bytesWritten, Update.errorString());
    char detail[48];
    snprintf(detail, sizeof(detail), "finish: %s", Update.errorString());
    webHookEvent("ota_failed", "", "", detail);
    return;
  }
  result = OtaResult::Done;
  rebootAtMs = millis() + OTA_REBOOT_DELAY_MS;
  debugLogPrintf("OTA: %u bytes written and verified; boot switch set\n",
                (unsigned)bytesWritten);
  char detail[24];
  snprintf(detail, sizeof(detail), "%u bytes", (unsigned)bytesWritten);
  webHookEvent("ota_success", "", "", detail);
}

void otaUploadAbort() {
  if (result != OtaResult::Uploading) return;
  Update.abort();
  result = OtaResult::Aborted;
  debugLogPrintf("OTA: upload aborted after %u bytes\n", (unsigned)bytesWritten);
}
