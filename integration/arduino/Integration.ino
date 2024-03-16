
#include <Arduino.h>
#include <EspNowCrypt.h>
#include <EspNowHost.h>
#include <EspNowNode.h>
#include <EspNowPreferences.h>
#include <driver/gpio.h>
#include <esp_crt_bundle.h>

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

String logLevelToString(const esp_log_level_t log_level) {
  String level;
  switch (log_level) {
  case ESP_LOG_NONE:
    level = "none";
    break;
  case ESP_LOG_ERROR:
    level = "error";
    break;
  case ESP_LOG_WARN:
    level = "warning";
    break;
  case ESP_LOG_INFO:
    level = "info";
    break;
  case ESP_LOG_DEBUG:
    level = "debug";
    break;
  case ESP_LOG_VERBOSE:
    level = "verbose";
    break;
  default:
    level = "unknown";
    break;
  }
  return level;
}

EspNowHost::OnLog _on_host_log = [](const std::string message, const esp_log_level_t log_level) {
  Serial.println("EspNowHost (" + logLevelToString(log_level) + "): " + String(message.c_str()));
};

EspNowNode::OnLog _on_node_log = [](const std::string message, const esp_log_level_t log_level) {
  Serial.println("EspNowNode (" + logLevelToString(log_level) + "): " + String(message.c_str()));
};

EspNowNode::OnStatus _on_status = [](EspNowNode::Status status) {};

EspNowPreferences _esp_now_preferences;
EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowNode _esp_now_node(_esp_now_crypt, _esp_now_preferences, FIRMWARE_VERSION, _on_status, _on_host_log,
                         arduino_esp_crt_bundle_attach);
EspNowHost _esp_now_host(_esp_now_crypt, EspNowHost::WiFiInterface::STA, _on_new_message, _on_application_message,
                         _firmware_update_available, _on_node_log);

void setup() {
  Serial.begin(115200);

  _esp_now_host.setup();
  _esp_now_preferences.initalizeNVS();
  _esp_now_node.setup();
}

void loop() {}
