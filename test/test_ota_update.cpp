#include "check.h"
#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>

// Definition for the externs in our shims
MockUpdateClass Update;
esp_partition_t mock_running_partition = {"app0"};
esp_partition_t* mock_running_partition_ptr = &mock_running_partition;
esp_ota_img_states_t mock_partition_state = ESP_OTA_IMG_VALID;
bool mock_mark_app_valid_called = false;

// Mocks for dependencies
bool mock_registry_flush_called = false;

#define registryFlush mock_registry_flush
void mock_registry_flush() { mock_registry_flush_called = true; }

#define webHookEvent mockWebHookEvent
static String mock_webhook_name;
static String mock_webhook_detail;
bool mockWebHookEvent(const char* name, const char*, const char*, const char* detail) {
  mock_webhook_name = name;
  mock_webhook_detail = detail;
  return true;
}

#define millis mock_millis
static uint32_t mock_millis_val = 0;
uint32_t mock_millis() { return mock_millis_val; }

#define delay mock_delay
static uint32_t mock_delay_ms = 0;
void mock_delay(uint32_t ms) { mock_delay_ms = ms; }

// ESP mock
struct ESPClass {
  bool restart_called = false;
  void restart() { restart_called = true; }
};
ESPClass ESP;

#include "../ota_update.cpp"

static void resetMocks() {
  mock_partition_state = ESP_OTA_IMG_VALID;
  mock_mark_app_valid_called = false;
  Update.begin_ret = true;
  Update.end_ret = true;
  Update.write_ret = 0;
  Update.error_str = "";
  Update.md5 = "";
  Update.abort_called = false;
  mock_registry_flush_called = false;
  mock_webhook_name = "";
  mock_webhook_detail = "";
  mock_millis_val = 0;
  mock_delay_ms = 0;
  ESP.restart_called = false;

  result = OtaResult::Idle;
  bytesWritten = 0;
  rebootAtMs = 0;
  validating = false;
  validatingSinceMs = 0;
}

void runOtaUpdateTests() {
  check::begin("ota_update tests");
  resetMocks();

  // Test basic OTA upload start
  otaUploadStart("");
  CHECK_EQ(result == OtaResult::Uploading, true);
  CHECK_EQ(mock_registry_flush_called, true);

  // Test ota uploading and bytes written
  CHECK_EQ(otaUploading(), true);
  CHECK_EQ(otaBytesWritten(), (size_t)0);

  // Test OTA start fails if Update.begin fails
  resetMocks();
  Update.begin_ret = false;
  otaUploadStart("");
  CHECK_EQ(result == OtaResult::BeginFailed, true);
  CHECK_EQ(mock_webhook_name, "ota_failed");

  // Test ota reject
  resetMocks();
  otaUploadReject();
  CHECK_EQ(result == OtaResult::Rejected, true);
  CHECK_EQ(mock_webhook_name, "ota_rejected");

  // Test OTA write success
  resetMocks();
  otaUploadStart("");
  Update.write_ret = 100;
  uint8_t data[100];
  otaUploadWrite(data, 100);
  CHECK_EQ(bytesWritten, (size_t)100);

  // Test OTA write fail
  resetMocks();
  otaUploadStart("");
  Update.write_ret = 50;
  otaUploadWrite(data, 100);
  CHECK_EQ(result == OtaResult::WriteFailed, true);
  CHECK_EQ(Update.abort_called, true);
  CHECK_EQ(mock_webhook_name, "ota_failed");

  // Test OTA end success
  resetMocks();
  otaUploadStart("");
  otaUploadEnd();
  CHECK_EQ(result == OtaResult::Done, true);
  CHECK_EQ(mock_webhook_name, "ota_success");

  // Test OTA end fail
  resetMocks();
  otaUploadStart("");
  Update.end_ret = false;
  otaUploadEnd();
  CHECK_EQ(result == OtaResult::FinishFailed, true);
  CHECK_EQ(mock_webhook_name, "ota_failed");

  // Test OTA abort
  resetMocks();
  otaUploadStart("");
  otaUploadAbort();
  CHECK_EQ(result == OtaResult::Aborted, true);
  CHECK_EQ(Update.abort_called, true);

  // Test pending verify
  resetMocks();
  mock_partition_state = ESP_OTA_IMG_PENDING_VERIFY;
  CHECK_EQ(otaPendingVerify(), true);

  // Test otaBegin and otaTick
  resetMocks();
  mock_partition_state = ESP_OTA_IMG_PENDING_VERIFY;
  otaBegin();
  CHECK_EQ(validating, true);

  mock_millis_val = OTA_VALIDATION_MS; // advance time past validation
  otaTick();
  CHECK_EQ(validating, false);
  CHECK_EQ(mock_mark_app_valid_called, true);
}
