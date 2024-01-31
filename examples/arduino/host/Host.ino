#include <Arduino.h>
#include <EspNowCrypt.h>
#include <EspNowHost.h>
#include <WiFi.h>
#include <esp_wifi.h>

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

// Change this to your LED pin
#define LED_PIN GPIO_NUM_15

const char wifi_ssid[] = "my-wifi-ssid";
const char wifi_password[] = "my-wifi-password";
// Encyption key used for our own packet encryption (GCM).
// We are not using the esp-now encryption due to the peer limitation.
// The key should be the same for both the host and the node.
const char esp_now_encryption_key[] = "0123456789ABCDEF"; // Must be exact 16 bytes long. \0 does not count.

// Used to validate the integrity of the messages.
// The secret should be the same for both the host and the node.
const char esp_now_encryption_secret[] = "01234567"; // Must be exact 8 bytes long. \0 does not count.

unsigned long _turn_of_led_at_ms = 0;

EspNowHost::OnNewMessage _on_new_message = []() {
  // Callback on any new message. Turn on led to indicate this.
  digitalWrite(LED_PIN, HIGH);
  _turn_of_led_at_ms = millis() + 1000;
};

EspNowHost::OnApplicationMessage _on_application_message = [](EspNowHost::MessageMetadata metadata,
                                                              const uint8_t *message) {
  // Callback for new application messages.
  auto id = message[0];
  Serial.println("Got message from 0x" + String(metadata.mac_address) + " with ID " + id + " rssi=" + String(metadata.rssi));
  switch (id) {
  case 0x01: {
    MyApplicationMessage *msg = (MyApplicationMessage *)message;
    Serial.println("MyApplicationMessage::open: " + String(msg->open));
    break;
  }
  case 0x02: {
    MySecondApplicationMessage *msg = (MySecondApplicationMessage *)message;
    Serial.println("MyApplicationMessage::temperature: " + String(msg->temperature));
    break;
  }
  }
};

EspNowHost::FirmwareUpdateAvailable _firmware_update_available = [](uint64_t mac_address, uint32_t firmware_version) {
  // Callback where we can indicate if there is a new firmware available.
  return std::nullopt;
};

EspNowHost::OnLog _on_log = [](const std::string message, const esp_log_level_t log_level) {
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

  Serial.println(("EspNowHost (" + level + "): " + message).c_str());
};

EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowHost _esp_now_host(_esp_now_crypt, EspNowHost::WiFiInterface::STA, _on_new_message, _on_application_message,
                         _firmware_update_available, _on_log);

void setup() {
  Serial.begin(115200);

  // Setup WiFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("have wifi");
  Serial.print("IP number: ");
  Serial.println(WiFi.localIP());

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
  _turn_of_led_at_ms = millis() + 1000;

  esp_wifi_set_ps(WIFI_PS_NONE); // No sleep on WiFi to be able to receive ESP-NOW packages without being in AP mode.

  _esp_now_host.setup();
}

void loop() {
  if (_turn_of_led_at_ms > 0 && millis() > _turn_of_led_at_ms) {
    digitalWrite(LED_PIN, LOW);
    _turn_of_led_at_ms = -1;
  }
}
