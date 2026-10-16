#pragma once

#include <stdint.h>

#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NOT_FOUND 2
typedef int esp_err_t;

inline const char* esp_err_to_name(esp_err_t code) {
  if (code == ESP_OK) return "ESP_OK";
  return "ERROR";
}

struct esp_partition_t {
  const char* label;
};

#define ESP_OTA_IMG_NEW 0
#define ESP_OTA_IMG_PENDING_VERIFY 1
#define ESP_OTA_IMG_VALID 2
#define ESP_OTA_IMG_INVALID 3
#define ESP_OTA_IMG_ABORTED 4
#define ESP_OTA_IMG_UNDEFINED 5
typedef uint32_t esp_ota_img_states_t;

extern esp_partition_t mock_running_partition;
extern esp_partition_t* mock_running_partition_ptr;
inline const esp_partition_t* esp_ota_get_running_partition() {
  return mock_running_partition_ptr;
}

extern esp_ota_img_states_t mock_partition_state;
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t* partition, esp_ota_img_states_t* ota_state) {
  if (!partition) return ESP_ERR_INVALID_ARG;
  *ota_state = mock_partition_state;
  return ESP_OK;
}

extern bool mock_mark_app_valid_called;
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback() {
  mock_mark_app_valid_called = true;
  return ESP_OK;
}
