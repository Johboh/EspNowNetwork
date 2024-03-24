#include <EspNowCrypt.h>
#include <EspNowHost.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "example"

// Change this to your LED pin
#define LED_PIN GPIO_NUM_15

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
  gpio_set_level(LED_PIN, 1);
};

EspNowHost::OnApplicationMessage _on_application_message = [](EspNowHost::MessageMetadata metadata,
                                                              const uint8_t *message) {
  // Callback for new application messages.
  auto id = message[0];
  ESP_LOGI(TAG, "Got message from 0x%02llx with ID %d", metadata.mac_address, id);
  switch (id) {
  case 0x01: {
    MyApplicationMessage *msg = (MyApplicationMessage *)message;
    ESP_LOGI(TAG, "MyApplicationMessage::open: %d", msg->open);
    break;
  }
  case 0x02: {
    MySecondApplicationMessage *msg = (MySecondApplicationMessage *)message;
    ESP_LOGI(TAG, "MyApplicationMessage::temperature: %f", msg->temperature);
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
  esp_log_write(log_level, TAG, "EspNowNode: %s\n", message.c_str());
};

EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
EspNowHost _esp_now_host(_esp_now_crypt, {.wifi_interface = EspNowHost::WiFiInterface::STA}, _on_new_message,
                         _on_application_message, _firmware_update_available, _on_log);

extern "C" {
void app_main();
}

void app_main(void) {
  if (!_esp_now_host.start()) {
    ESP_LOGE(TAG, "Failed to start host.");
  } else {
    ESP_LOGE(TAG, "Host started.");
  }

  while (1) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}
