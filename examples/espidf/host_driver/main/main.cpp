#include "DeviceFootPedal.h"
#include "esp-now-device-structs.h"
#include <Device.h>
#include <DeviceManager.h>
#include <FirmwareChecker.h>
#include <FirmwareKicker.h>
#include <HostDriver.h>
#include <MQTTRemote.h>
#include <OtaHelper.h>
#include <WiFiHelper.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <optional>
#include <string>

/*
 * This example depend on the following libraries:
 * johboh/MQTTRemote - for the MQTT implementation.
 * johboh/ConnectionHelper - For host OTA and WiFi setup (not for nodes OTA)
 *
 * See idf_component.yml for exact dependencies.
 */

using namespace std::placeholders;

#define TAG "example"

const char hostname[] = "my-host-driver";
const char wifi_ssid[] = "my-wifi-ssid";
const char wifi_password[] = "my-wifi-password";
const char mqtt_client_id[] = "my-host-driver";
const char mqtt_host[] = "192.168.1.100";
const char firmware_update_base_url[] = "http://192.168.1.100:8080/";
const char mqtt_username[] = "mqtt-username";
const char mqtt_password[] = "mqtt-password";
const int mqtt_port = 1883;
const int firmware_kicker_port = 82;

// Encyption key used for our own packet encryption (GCM).
// We are not using the esp-now encryption due to the peer limitation.
// The key should be the same for both the host and the node.
const char esp_now_encryption_key[] = "0123456789ABCDEF"; // Must be exact 16 bytes long. \0 does not count.

// Used to validate the integrity of the messages.
// The secret should be the same for both the host and the node.
const char esp_now_encryption_secret[] = "01234567"; // Must be exact 8 bytes long. \0 does not count.

// OTA, WIFI and MQTT for host.
OtaHelper::Configuration ota_configuration = {
    .web_ota =
        {
            .id = hostname,
        },
};
OtaHelper _ota_helper(ota_configuration);
WiFiHelper _wifi_helper(hostname);
MQTTRemote _mqtt_remote(mqtt_client_id, mqtt_host, mqtt_port, mqtt_username, mqtt_password);

// Add Two foot pedals, one left and one right.
// We have these as physical nodes.
DeviceFootPedal _device_foot_pedal_left(_mqtt_remote, 0x543204017648, "Left", [](uint8_t click) {
  switch (click) {
  case CLICK_SINGLE:
    // TODO: Handle click
    break;

  case CLICK_LONG:
    // TODO: Handle long click
    break;
  }
});
DeviceFootPedal _device_foot_pedal_right(_mqtt_remote, 0x543204016bfc, "Right", [](uint8_t click) {
  switch (click) {
  case CLICK_SINGLE:
    // TODO: Handle click
    break;

  case CLICK_LONG:
    // TODO: Handle long click
    break;
  }
});

// List all devices.
std::vector<std::reference_wrapper<Device>> _devices{_device_foot_pedal_left, _device_foot_pedal_right};

// Create Device Manager and Firmware Checker and register devices.
DeviceManager _device_manager(_devices, []() { return _mqtt_remote.connected(); });
FirmwareChecker _firmware_checker(firmware_update_base_url, _devices, {.check_every_ms = 30000});
FirmwareKicker _firmware_kicker(_firmware_checker, firmware_kicker_port);

// Setup host driver.
HostDriver _host_driver(_device_manager,
                        {
                            .wifi_ssid = wifi_ssid,
                            .wifi_password = wifi_password,
                            .esp_now_encryption_key = esp_now_encryption_key,
                            .esp_now_encryption_secret = esp_now_encryption_secret,
                        },
                        [](const std::string message, const std::string sub_path, const bool retain) {
                          _mqtt_remote.publishMessage(_mqtt_remote.clientId() + sub_path, message, retain);
                        });

extern "C" {
void app_main();
}

void app_main(void) {

  // Connect to WIFI
  auto connected = _wifi_helper.connectToAp(wifi_ssid, wifi_password, true, 10000);
  if (connected) {
    // Connected to WIFI

    esp_wifi_set_ps(WIFI_PS_NONE); // No sleep on WiFi to be able to receive ESP-NOW packages without being in AP mode.

    // Start OTA
    if (!_ota_helper.start()) {
      ESP_LOGE(TAG, "Failed to start OTA");
    }

    // Start MQTT
    _mqtt_remote.start();

    // Start host driver with Firmware Checker and Firmware Kicker (both optional)
    _host_driver.setup(_firmware_checker, _firmware_kicker);

    // Start task for the device manager and firmware checker and start firmware kicker
    _device_manager.startTask();
    _firmware_checker.startTask();
    _firmware_kicker.start();
  } else {
    ESP_LOGE(TAG, "Failed to connect to WiFI");
  }

  while (1) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
    fflush(stdout);
  }
}
