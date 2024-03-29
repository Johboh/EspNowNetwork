#include <EspNowCrypt.h>
#include <EspNowHost.h>
#include <EspNowNode.h>
#include <EspNowPreferences.h>
#include <driver/gpio.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "example"

/**
 * This is only a verification build of the full library. Not representable for a node or a host. For that, check
 * examples instead.
 */

#define FIRMWARE_VERSION 1

const char esp_now_encryption_key[] = "0123456789ABCDEF"; // Must be exact 16 bytes long. \0 does not count.
const char esp_now_encryption_secret[] = "01234567";      // Must be exact 8 bytes long. \0 does not count.

EspNowHost::OnNewMessage _on_new_message = []() {};

EspNowHost::OnApplicationMessage _on_application_message = [](EspNowHost::MessageMetadata metadata,
                                                              const uint8_t *message) {};

EspNowHost::FirmwareUpdateAvailable _firmware_update_available = [](uint64_t mac_address, uint32_t firmware_version) {
  return std::nullopt;
};

EspNowHost::OnLog _on_host_log = [](const std::string message, const esp_log_level_t log_level) {
  esp_log_write(log_level, TAG, "EspNowHost: %s\n", message.c_str());
};

EspNowNode::OnLog _on_node_log = [](const std::string message, const esp_log_level_t log_level) {
  esp_log_write(log_level, TAG, "EspNowNode: %s", message.c_str());
};

EspNowNode::OnStatus _on_status = [](EspNowNode::Status status) {};

EspNowPreferences _esp_now_preferences;
EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowNode _esp_now_node(_esp_now_crypt, _esp_now_preferences, FIRMWARE_VERSION, _on_status, _on_host_log,
                         esp_crt_bundle_attach);
EspNowHost _esp_now_host(_esp_now_crypt, {.wifi_interface = EspNowHost::WiFiInterface::STA}, _on_new_message,
                         _on_application_message, _firmware_update_available, _on_node_log);

extern "C" {
void app_main();
}

void app_main(void) {
  if (!_esp_now_host.start()) {
    ESP_LOGE(TAG, "Failed to start host.");
  } else {
    ESP_LOGE(TAG, "Host started.");
  }

  _esp_now_preferences.initalizeNVS();

  if (_esp_now_node.setup()) {
    ESP_LOGE(TAG, "Node setup.");
  }

  while (1) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}
