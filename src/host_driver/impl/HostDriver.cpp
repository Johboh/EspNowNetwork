#include "HostDriver.h"
#include <cstring>

using namespace std::placeholders;

HostDriver::HostDriver(IDeviceManager &device_manager, const char *wifi_ssid, const char *wifi_password,
                       const char *esp_now_encryption_key, const char *esp_now_encryption_secret, OnLog on_log,
                       OnMessage on_message)
    : _on_log(on_log), _on_message(on_message), _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret),
      _esp_now_host(_esp_now_crypt, EspNowHost::WiFiInterface::STA, std::bind(&HostDriver::onNewMessage, this),
                    std::bind(&HostDriver::onNewApplicationMessage, this, _1, _2),
                    std::bind(&HostDriver::onFirmwareUpdate, this, _1, _2, wifi_ssid, wifi_password),
                    std::bind(&HostDriver::onHostLog, this, _1, _2)),
      _device_manager(device_manager) {
  _device_manager.addOnLog(std::bind(&HostDriver::onDeviceManagerLog, this, _1, _2));
}

void HostDriver::setup(std::optional<std::reference_wrapper<IFirmwareChecker>> firmware_checker) {
  _esp_now_host.setup();
  _firmware_checker = firmware_checker;
  if (_firmware_checker) {
    auto &firmware_checker = _firmware_checker.value().get();
    firmware_checker.addOnLog(std::bind(&HostDriver::onFirwmareLog, this, _1, _2));
    firmware_checker.addOnAvailableFirmware(std::bind(&HostDriver::onAvailableFirwmare, this, _1, _2, _3, _4));
  }
}

std::string HostDriver::logLevelToString(const esp_log_level_t log_level) {
  switch (log_level) {
  case ESP_LOG_NONE:
    return "none";
  case ESP_LOG_ERROR:
    return "error";
  case ESP_LOG_WARN:
    return "warning";
  case ESP_LOG_INFO:
    return "info";
  case ESP_LOG_DEBUG:
    return "debug";
  case ESP_LOG_VERBOSE:
    return "verbose";
  default:
    break;
  }
  return "unknown";
}

void HostDriver::onNewMessage() {
  if (_on_message) {
    _on_message();
  }
}

void HostDriver::onHostLog(const std::string message, const esp_log_level_t log_level) {
  if (log_level == ESP_LOG_NONE) {
    return; // Weird flex, but ok
  }

  std::string level = logLevelToString(log_level);

  log("/log/" + level, "[#" + std::to_string(_log_messages++) + "] " + message);
}

void HostDriver::onFirwmareLog(const std::string message, const esp_log_level_t log_level) {
  if (log_level == ESP_LOG_NONE) {
    return; // Weird flex, but ok
  }

  std::string level = logLevelToString(log_level);

  log("/firmware/log/" + level, "[#" + std::to_string(_log_messages++) + "] " + message);
}

void HostDriver::onAvailableFirwmare(const std::string device_type, const std::optional<std::string> device_hardware,
                                     const uint32_t firmware_version, const std::string firmware_md5) {
  log("/firmware/available/" + device_type + (device_hardware ? "/" + device_hardware.value() : ""),
      std::to_string(firmware_version));
}

void HostDriver::onDeviceManagerLog(const std::string message, const esp_log_level_t log_level) {
  if (log_level == ESP_LOG_NONE) {
    return; // Weird flex, but ok
  }

  std::string level = logLevelToString(log_level);

  log("/log/" + level, "[#" + std::to_string(_log_messages++) + "] " + message);
}

void HostDriver::onNewApplicationMessage(EspNowHost::MessageMetadata metadata, const uint8_t *message) {
  _device_manager.forward(metadata.retries, metadata.mac_address, message);
}

std::optional<EspNowHost::FirmwareUpdate> HostDriver::onFirmwareUpdate(uint64_t mac_address, uint32_t firmware_version,
                                                                       const char *wifi_ssid,
                                                                       const char *wifi_password) {
  if (!_firmware_checker) {
    return std::optional<EspNowHost::FirmwareUpdate>{std::nullopt};
  }

  auto optdev = _device_manager.deviceForMac(mac_address);
  if (!optdev) {
    return std::optional<EspNowHost::FirmwareUpdate>{std::nullopt};
  }
  auto type = optdev->get().type();
  auto name = optdev->get().name();
  auto hardware = optdev->get().hardware();
  auto firmware_mqtt_path =
      "/firmware/current/" + type + (hardware ? "/" + hardware.value() : "") + "/" + makeMqttPathCompatible(name);

  // Do we have a newer firmware version for this type?
  auto update_information = _firmware_checker.value().get().getUpdateUrl(firmware_version, type, hardware);
  if (update_information) {
    log(firmware_mqtt_path, "Updating to " + std::to_string(update_information->version), true);

    EspNowHost::FirmwareUpdate firmware_update;
    std::strncpy(firmware_update.wifi_ssid, wifi_ssid, sizeof(firmware_update.wifi_ssid));
    std::strncpy(firmware_update.wifi_password, wifi_password, sizeof(firmware_update.wifi_password));
    std::strncpy(firmware_update.url, update_information->url.c_str(), sizeof(firmware_update.url));
    std::strncpy(firmware_update.md5, update_information->md5.c_str(), sizeof(firmware_update.md5));
    return std::optional<EspNowHost::FirmwareUpdate>{firmware_update};
  }

  log(firmware_mqtt_path, std::to_string(firmware_version), true);

  return std::optional<EspNowHost::FirmwareUpdate>{std::nullopt};
}

std::string HostDriver::makeMqttPathCompatible(const std::string &input) {
  std::string result;
  for (char c : input) {
    if (c != '+' && c != '#' && c != '/') {
      result.push_back(c);
    }
  }
  return result;
}

void HostDriver::log(const std::string sub_path, const std::string message, const bool retain) {
  if (_on_log) {
    _on_log(message, sub_path, retain);
  }
}