#include <EspNowPreferences.h>
#include <esp_system.h>
#include <esp_vfs.h>
#include <esp_vfs_fat.h>
#include <nvs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#define TAG "ESP_NOW_PREFERENCES"

#define NVS_STORAGE "storage"
// Max key length: 15 chars
#define NVS_STORAGE_KEY_HAVE_MAC "have_mac"
#define NVS_STORAGE_KEY_HOST_MAC "host_mac"

EspNowPreferences::EspNowPreferences() {}

void EspNowPreferences::init() {
  ESP_LOGI(TAG, "Initialize NVS");
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGE(TAG, "Erasing nvs (%s)", esp_err_to_name(err));
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

void EspNowPreferences::setHasMac(bool have) { setBool(NVS_STORAGE_KEY_HAVE_MAC, have); }
bool EspNowPreferences::hasMac() { return getBool(NVS_STORAGE_KEY_HAVE_MAC); }

bool EspNowPreferences::setMac(uint8_t *mac, size_t length) {
  auto key = NVS_STORAGE_KEY_HOST_MAC;
  esp_err_t err = nvs_set_blob(_nvs_handle, key, mac, length);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set blob to NVS with key %s (%s)", key, esp_err_to_name(err));
  }
  return err == ESP_OK;
}

size_t EspNowPreferences::getMacLength() {
  auto key = NVS_STORAGE_KEY_HOST_MAC;
  size_t required_size;
  esp_err_t err = nvs_get_blob(_nvs_handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get required size for blob from NVS with key %s (%s)", key, esp_err_to_name(err));
    return 0;
  }
  return required_size;
}

bool EspNowPreferences::getMac(uint8_t *buffer, size_t length) {
  auto key = NVS_STORAGE_KEY_HOST_MAC;
  if (buffer == nullptr) {
    ESP_LOGE(TAG, "mac buffer is null");
    return false;
  }

  auto required_size = getMacLength();
  if (length == 0 || length < required_size) {
    ESP_LOGE(TAG, "Length of buffer is zero or is too small.");
    return false;
  }

  esp_err_t err = nvs_get_blob(_nvs_handle, key, buffer, &required_size);
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

uint8_t EspNowPreferences::getU8(const std::string &key) {
  uint8_t r = 0;
  esp_err_t err = nvs_get_u8(_nvs_handle, key.c_str(), &r);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get u8 from NVS with key %s (%s)", key.c_str(), esp_err_to_name(err));
    return 0;
  }
  return r;
}
void EspNowPreferences::setU8(const std::string &key, const uint8_t value, bool commit) {
  esp_err_t err = nvs_set_u8(_nvs_handle, key.c_str(), value);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set u8 to NVS with key %s (%s)", key.c_str(), esp_err_to_name(err));
  } else if (commit) {
    EspNowPreferences::commit();
  }
}