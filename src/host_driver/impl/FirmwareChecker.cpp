#include "FirmwareCheckerUtils.h"
#include <FirmwareChecker.h>
#include <cstdlib>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define CHECK_NOW_BIT 1

FirmwareChecker::FirmwareChecker(std::string base_url, const std::vector<std::reference_wrapper<Device>> &devices,
                                 Configuration configuration)
    : _base_url(base_url), _configuration(configuration) {
  for (const auto &device_ref : devices) {
    auto &device = device_ref.get();
    _available_devices.emplace(FirmwareDevice{device.type(), device.hardware()});
  }
  _check_now_event_group = xEventGroupCreate();
}

void FirmwareChecker::run_task(void *pvParams) {
  while (1) {
    FirmwareChecker *_this = (FirmwareChecker *)pvParams;
    _this->checkFirmware();
    // Wait for triggered check, or timeout which is the normal check period time.
    auto period = _this->_configuration.check_every_ms / portTICK_PERIOD_MS;
    xEventGroupWaitBits(_this->_check_now_event_group, CHECK_NOW_BIT, pdTRUE, pdFALSE, period);
    taskYIELD();
  }
}

void FirmwareChecker::startTask(unsigned long task_size, uint8_t task_priority) {
  xTaskCreate(&run_task, "firmware_checker_task", task_size, this, task_priority, NULL);
}

void FirmwareChecker::handle() {
  auto now = xTaskGetTickCount() * portTICK_PERIOD_MS;
  if (now - _checked_device_last_at_ms > _configuration.check_every_ms) {
    checkFirmware();
    _checked_device_last_at_ms = now;
  }
}

void FirmwareChecker::checkFirmware() {

  // Have specific device to check now?
  if (_device_to_check_now) {
    checkFirmware(_device_to_check_now.value());
    _device_to_check_now = std::nullopt;
  } else {
    // Normal flow.
    if (_available_devices.empty()) {
      log("No available types to check.", ESP_LOG_WARN);
      return;
    }

    if (_devices_iterator == _available_devices.end()) {
      _devices_iterator = _available_devices.begin();
    }

    // Get current time and go to next one for next round.
    auto device = *_devices_iterator;
    ++_devices_iterator;

    checkFirmware(device);
  }
}

void FirmwareChecker::checkFirmware(FirmwareDevice &device) {
  auto hardware_opt = device.hardware;
  std::string md5_url = _base_url + device.type + (hardware_opt ? "/" + hardware_opt.value() : "") + "/firmware.md5";
  std::string version_url =
      _base_url + device.type + (hardware_opt ? "/" + hardware_opt.value() : "") + "/firmware_version.txt";

  auto device_and_hardware_str = "type " + device.type + (hardware_opt ? " and hardware " + hardware_opt.value() : "");

  log("Checking for " + device_and_hardware_str + " using URL " + version_url, ESP_LOG_INFO);

  std::optional<uint32_t> version = std::nullopt;
  std::optional<std::string> md5 = std::nullopt;

  auto version_string = FirmwareCheckerUtils::getContentStringForUrl(version_url);
  if (version_string && !version_string.value().empty()) {
    uint32_t version_from_string = strtol(version_string.value().c_str(), nullptr, 10);
    if (version_from_string > 0) {
      version = version_from_string;
      log("Got firmware version for " + device_and_hardware_str + ": " + std::to_string(version.value()), ESP_LOG_INFO);
    } else {
      log("Got invalid firmware version for " + device_and_hardware_str + ": " + version_string.value(), ESP_LOG_WARN);
    }
  } else {
    log("Failed to get version for " + device_and_hardware_str, ESP_LOG_WARN);
  }

  md5 = FirmwareCheckerUtils::getContentStringForUrl(md5_url);
  if (md5) {
    log("Got firmware md5 for " + device_and_hardware_str + ": " + md5.value(), ESP_LOG_INFO);
  } else {
    log("Failed to get firmware md5 for " + device_and_hardware_str, ESP_LOG_WARN);
  }

  if (version && md5) {
    _firmware_version_for_device[device] = Firmware{version.value(), md5.value()};
    for (auto &on_available_firmware : _on_available_firmware) {
      if (on_available_firmware) {
        on_available_firmware(device.type, device.hardware, version.value(), md5.value());
      }
    }
  } else {
    // On failure, clear.
    _firmware_version_for_device.erase(device);
    log("Unable to get firmware version or md5 for " + device_and_hardware_str, ESP_LOG_WARN);
  }
}

std::optional<FirmwareChecker::UpdateInformation> FirmwareChecker::getUpdateUrl(uint32_t version, std::string type,
                                                                                std::optional<std::string> hardware) {
  auto rlst = _firmware_version_for_device.find(FirmwareDevice{type, hardware});
  if (rlst != _firmware_version_for_device.end()) {
    auto available_firmware_version = rlst->second.version;
    if (available_firmware_version > version) {
      std::string url = _base_url + type + (hardware ? "/" + hardware.value() : "") + "/firmware.bin";
      return UpdateInformation{url, rlst->second.md5, available_firmware_version};
    }
  }
  // Did not find any version at all.
  return std::nullopt;
}

void FirmwareChecker::checkNow(const std::string device_type, const std::optional<std::string> device_hardware) {
  _device_to_check_now = FirmwareDevice{
      .type = device_type,
      .hardware = device_hardware,
  };

  // Force check for manual polling.
  _checked_device_last_at_ms = 0;
  // Force check for threaded polling.
  xEventGroupSetBits(_check_now_event_group, CHECK_NOW_BIT);
}

void FirmwareChecker::log(const std::string message, const esp_log_level_t log_level) {
  for (auto &on_log : _on_log) {
    if (on_log) {
      on_log(message, log_level);
    }
  }
}