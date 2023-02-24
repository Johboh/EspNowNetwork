#include <Arduino.h>
#include <EspNowCrypt.h>
#include <EspNowHost.h>
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#else
#error "Unsupported hardware. Sorry!"
#endif

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

EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowHost _esp_now_host(
    _esp_now_crypt,
    []() {
      // Callback on any new message. Turn on led to indicate this.
      digitalWrite(LED_PIN, HIGH);
      _turn_of_led_at_ms = millis() + 1000;
    },
    [](EspNowHost::MessageMetadata metadata, const uint8_t *message) {
      // Callback for new application messages.
      auto id = message[0];
      Serial.println("Got message from 0x" + String(metadata.mac_address) + " with ID " + id);
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
    },
    [](const String message, const esp_log_level_t log_level) {
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

      Serial.println("EspNowHost (" + level + "): " + message);
    });

void setup() {
  Serial.begin(115200);

  // Setup WiFI
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

  _esp_now_host.setup();
}

void loop() {
  _esp_now_host.handle();

  if (_turn_of_led_at_ms > 0 && millis() > _turn_of_led_at_ms) {
    digitalWrite(LED_PIN, LOW);
    _turn_of_led_at_ms = -1;
  }
}
