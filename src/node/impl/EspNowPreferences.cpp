#include <EspNowPreferences.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs.h>

#define TAG "ESP_NOW_PREFERENCES"

#define NVS_STORAGE "storage"
// Max key length: 15 chars
#define NVS_STORAGE_KEY_HOST_MAC "host_mac"
#define NVS_STORAGE_KEY_HOST_CHAN "host_channel"

EspNowPreferences::EspNowPreferences() {}

void EspNowPreferences::initalizeNVS() {
  ESP_LOGI(TAG, "Initializing NVS");
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE(TAG, "Erasing NVS (%s)", esp_err_to_name(err));
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  err = nvs_open(NVS_STORAGE, NVS_READWRITE, &_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS on storage (%s)", esp_err_to_name(err));
    return;
  }
}

bool EspNowPreferences::espNowSetChannelForHost(uint8_t channel) {
  auto key = NVS_STORAGE_KEY_HOST_CHAN;
  esp_err_t err = nvs_set_u8(_nvs_handle, key, channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set blob to NVS with key %s (%s)", key, esp_err_to_name(err));
  }
  return err == ESP_OK;
}

bool EspNowPreferences::espNowGetChannelForHost(uint8_t *channel) {
  auto key = NVS_STORAGE_KEY_HOST_CHAN;

  esp_err_t err = nvs_get_u8(_nvs_handle, key, channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get u8 from NVS with key %s (%s)", key, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool EspNowPreferences::espNowSetMacForHost(uint8_t mac[MAC_ADDRESS_LENGTH]) {
  auto key = NVS_STORAGE_KEY_HOST_MAC;
  esp_err_t err = nvs_set_blob(_nvs_handle, key, mac, MAC_ADDRESS_LENGTH);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set blob to NVS with key %s (%s)", key, esp_err_to_name(err));
  }
  return err == ESP_OK;
}

bool EspNowPreferences::espNowGetMacForHost(uint8_t *buffer) {
  auto key = NVS_STORAGE_KEY_HOST_MAC;
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "mac buffer is null");
    return false;
  }

  size_t required_size;
  esp_err_t err = nvs_get_blob(_nvs_handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get required size for blob from NVS with key %s (%s)", key, esp_err_to_name(err));
    return false;
  }
  if (required_size != MAC_ADDRESS_LENGTH) {
    ESP_LOGE(TAG, "Length of buffer stored in memory is not MAC address size of MAC_ADDRESS_LENGTH(%d), was %d",
             MAC_ADDRESS_LENGTH, required_size);
    return false;
  }

  err = nvs_get_blob(_nvs_handle, key, buffer, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get blob from NVS with key %s (%s)", key, esp_err_to_name(err));
    return false;
  }
  return true;
}

bool EspNowPreferences::commit() {
  esp_err_t err = nvs_commit(_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS (%s)", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool EspNowPreferences::eraseAll() {
  esp_err_t err = nvs_erase_all(_nvs_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to erase NVS (%s)", esp_err_to_name(err));
    return false;
  }
  return true;
}
