#include "DeviceUtils.h"
#include <DeviceManager.h>

DeviceManager::DeviceManager(std::vector<std::reference_wrapper<Device>> &devices, IsConnected is_connected)
    : _is_connected(is_connected) {
  for (const auto &device_ref : devices) {
    auto &device = device_ref.get();
    _devices.insert({device.macAddress(), device});
  }
}

void DeviceManager::handle() {
  // Handle all entities.
  for (const auto &device_pair : _devices) {
    auto &device = device_pair.second.get();
    device.handle(_last_message_ms[device.macAddress()]);
  }

  auto connected = _is_connected && _is_connected();
  if (!_was_connected && connected) {
    for (const auto &device_pair : _devices) {
      auto &device = device_pair.second.get();
      device.onConnectionStateChanged(connected);
    }
  }
  _was_connected = connected;
}

void DeviceManager::forward(uint8_t retries, uint64_t mac_address, const uint8_t *message) {
  uint8_t version = message[0];

  if (auto device_wrapper = _devices.find(mac_address); device_wrapper != _devices.end()) {
    if (device_wrapper->second.get().onMessage(retries, version, message)) {
      _last_message_ms[mac_address] = DeviceUtils::millis();
    }

    log("Found device \"" + device_wrapper->second.get().name() + "\" for MAC address 0x" +
            DeviceUtils::toHex(mac_address),
        ESP_LOG_INFO);
  } else {
    log("No device with MAC address 0x" + DeviceUtils::toHex(mac_address) + " found.", ESP_LOG_WARN);
  }
}

std::optional<std::reference_wrapper<Device>> DeviceManager::deviceForMac(uint64_t mac_address) {
  if (auto device_wrapper = _devices.find(mac_address); device_wrapper != _devices.end()) {
    return std::optional<std::reference_wrapper<Device>>{device_wrapper->second};
  }
  return std::nullopt;
}

void DeviceManager::log(const std::string message, const esp_log_level_t log_level) {
  if (_on_log) {
    _on_log(message, log_level);
  }
}