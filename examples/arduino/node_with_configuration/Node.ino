#include <Arduino.h>
#include <EspNowCrypt.h>
#include <EspNowNode.h>
#include <EspNowPreferences.h>
#include <esp_crt_bundle.h>

#define MICROSECONDS_PER_SECOND 1000000LL
#define DEFAULT_SLEEP_TIME_US (MICROSECONDS_PER_SECOND * 60LL) // 60 seconds

#define FIRMWARE_VERSION 90201

// These structs are the application messages shared across the host and node
// device.
#pragma pack(1)
struct MyApplicationMessage {
  uint8_t id = 0x01;
  bool open;
};
struct MySecondApplicationMessage {
  uint8_t id = 0x02;
  double temperature;
};
struct MyConfigMessageV1 {
  uint32_t sleep_seconds; // how long the node should deep-sleep
  uint8_t foo;            // some other config value, not used in this example
};
#pragma pack(0)

// Encyption key used for our own packet encryption (GCM).
// We are not using the esp-now encryption due to the peer limitation.
// The key should be the same for both the host and the node.
const char esp_now_encryption_key[] = "0123456789ABCDEF"; // Must be exact 16 bytes long. \0 does not count.

// Used to validate the integrity of the messages.
// The secret should be the same for both the host and the node.
const char esp_now_encryption_secret[] = "01234567"; // Must be exact 8 bytes long. \0 does not count.

unsigned long _turn_of_led_at_ms = 0;

EspNowNode::OnLog _on_log = [](const std::string message, const esp_log_level_t log_level) {
  // Callback for logging. Can be omitted.
  if (log_level == ESP_LOG_NONE) {
    return; // Weird flex, but ok
  }

  std::string level;
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
  Serial.println(("EspNowNode (" + level + "): " + message).c_str());
};

EspNowNode::OnStatus _on_status = [](EspNowNode::Status status) {
  switch (status) {
  case EspNowNode::Status::HOST_DISCOVERY_STARTED:
    break;
  case EspNowNode::Status::HOST_DISCOVERY_SUCCESSFUL:
    break;
  case EspNowNode::Status::HOST_DISCOVERY_FAILED:
    break;
  case EspNowNode::Status::INVALID_HOST:
    break;
  case EspNowNode::Status::FIRMWARE_UPDATE_STARTED:
    break;
  case EspNowNode::Status::FIRMWARE_UPDATE_SUCCESSFUL:
    break;
  case EspNowNode::Status::FIRMWARE_UPDATE_FAILED:
    break;
  case EspNowNode::Status::FIRMWARE_UPDATE_WIFI_SETUP_FAILED:
    break;
  }
};

EspNowPreferences _esp_now_preferences;
EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowNode _esp_now_node(_esp_now_crypt, _esp_now_preferences, FIRMWARE_VERSION, _on_status, _on_log,
                         arduino_esp_crt_bundle_attach);

void setup() {
  Serial.begin(115200);

  _esp_now_preferences.initalizeNVS();

  MyConfigMessageV1 configuration;
  bool config_loaded = false;
  if (_esp_now_preferences.getConfigData((uint8_t *)&configuration, (uint8_t)sizeof(MyConfigMessageV1))) {
    config_loaded = true;
    _on_log(("loaded sleep_seconds=" + std::to_string(configuration.sleep_seconds) +
             " foo=" + std::to_string(configuration.foo))
                .c_str(),
            ESP_LOG_DEBUG);
  } else {
    _on_log("no config loaded", ESP_LOG_INFO);
  }

  // Setup node, send message, and then go to sleep.
  if (_esp_now_node.setup()) {
    MySecondApplicationMessage message;
    message.temperature = 25.6;
    _esp_now_node.sendMessage(&message, sizeof(MySecondApplicationMessage));
  } else {
    _on_log("setup failed", ESP_LOG_ERROR);
  }

  uint32_t sleep_time_us = DEFAULT_SLEEP_TIME_US;
  if (config_loaded) {
    sleep_time_us = configuration.sleep_seconds * MICROSECONDS_PER_SECOND;
  }

  _on_log(("sleeping for " + std::to_string(sleep_time_us / (MICROSECONDS_PER_SECOND)) + " seconds").c_str(),
          ESP_LOG_INFO);

  esp_deep_sleep(sleep_time_us);
}

void loop() {}