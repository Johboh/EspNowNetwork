#include <Arduino.h>
#include <EspNowCrypt.h>
#include <EspNowNode.h>

#define SLEEP_TIME_US (1000LL * 1000LL * 60LL * 1LL) // 1 minute

// These structs are the application messages shared across the host and node device.
#pragma pack(1)
struct MyApplicationMessage {
  uint8_t id = 0x01;
  bool open;
};
struct MySecondApplicationMessage {
  uint8_t id = 0x02;
  double temperature;
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

EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowNode _esp_now_node(_esp_now_crypt, [](const String message, const esp_log_level_t log_level) {
  // Callback for logging. Can be omitted.
  if (log_level == ESP_LOG_NONE) {
    return; // Weird flex, but ok
  }

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

  Serial.println("EspNowNode (" + level + "): " + message);
});

void setup() {
  Serial.begin(115200);

  // Setup node, send message, and then go to sleep.
  if (_esp_now_node.setup()) {
    MySecondApplicationMessage message;
    message.temperature = 25.6;
    _esp_now_node.sendMessage(&message, sizeof(MySecondApplicationMessage));
  }

  esp_deep_sleep(SLEEP_TIME_US);
}

void loop() {}