#ifndef __FIRMWARE_CHECKER_H__
#define __FIRMWARE_CHECKER_H__

#include "Device.h"
#include "IFirmwareChecker.h"
#include "esp_log.h"
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using OnAvailableFirmware = IFirmwareChecker::OnAvailableFirmware;
using OnLog = IFirmwareChecker::OnLog;

namespace FirmwareCheckerDefaults {
const uint32_t DEFAULT_CHECK_EACH_TYPE_EVERY_MS = 30000L; // Check one type every 30s
const uint32_t DEFAULT_STACK_SIZE = 4096;
const uint32_t DEFAULT_TASK_PRIORITY = 7;
} // namespace FirmwareCheckerDefaults

/**
 * @brief Periodically fetch the latest available firmware version.
 */
class FirmwareChecker : public IFirmwareChecker {
public:
  struct FirmwareDevice {
    std::string type;
    std::optional<std::string> hardware;

    bool operator==(const FirmwareDevice &other) const { return type == other.type && hardware == other.hardware; }
    bool operator<(const FirmwareDevice &other) const {
      if (hardware.has_value() && other.hardware.has_value()) {
        return type + hardware.value() < other.type + other.hardware.value();
      } else if (hardware.has_value()) {
        return type + hardware.value() < other.type;
      } else if (other.hardware.has_value()) {
        return type < other.type + other.hardware.value();
      } else {
        return type < other.type;
      }
    }
  };

  struct Configuration {
    /**
     * @brief how often to check one type and hardware combination, in milliseconds.
     */
    unsigned long check_every_ms = FirmwareCheckerDefaults::DEFAULT_CHECK_EACH_TYPE_EVERY_MS;
  };

  inline static Configuration _default = {.check_every_ms = FirmwareCheckerDefaults::DEFAULT_CHECK_EACH_TYPE_EVERY_MS};

  /**
   * @brief Construct a new Firmware Checker object
   *
   * @param base_url Base address on where to find firmware binary, version and optional MD5 hash. must end with a
   * trailing /. The device type and hardware (if present) will be appended as path to this path.
   *
   * The following must exist:
   * firmware.bin - the binary itself.
   * firmware_version.txt - containing the firmware version as a number.
   * firmware.md5 - the MD5 hash of the binary, as 32 hex characters.
   *
   * Example:
   * if the base_url is http://192.168.1.128/ the device type is "motion" and hardware is "lolin_c2",
   * then the final URL for getting firmware version will be http://192.168.1.128/motion/lolin_c2/firmware_version.txt
   * and the binary will be http://192.168.1.128/motion/lolin_c2/firmware.bin
   * The URL to the binary is what will be send to the node on OTA update.
   * If there is no hardware, the hardware part will be omitted and the final URL will be
   * http://192.168.1.128/motion/firmware_version.txt will be appended to this url, followed by firmware_version.txt.
   *
   * Note! No support for HTTPS as of now.
   *
   * @param devices all available type and (optional) hardware combinations.
   * @param configuration firmware checker specific configuration.
   */
  FirmwareChecker(std::string base_url, const std::vector<std::reference_wrapper<Device>> &devices,
                  Configuration configuration = _default);

public:
  /**
   * @brief Start a task that will drive the Firmware Checker. By calling this function, there is no
   * need to manually call the handle() function.
   */
  void startTask(unsigned long task_size = FirmwareCheckerDefaults::DEFAULT_STACK_SIZE,
                 uint8_t task_priority = FirmwareCheckerDefaults::DEFAULT_TASK_PRIORITY);

  /**
   * @brief Manually drive the Firmware Checker. Must be called perodically if startTask() was not called.
   */
  void handle();

  // See IFirmwareChecker
  void addOnLog(OnLog on_log) override { _on_log.push_back(on_log); }

  // See IFirmwareChecker
  void addOnAvailableFirmware(OnAvailableFirmware on_available_firmware) override {
    _on_available_firmware.push_back(on_available_firmware);
  }

  // See IFirmwareChecker
  std::optional<IFirmwareChecker::UpdateInformation> getUpdateUrl(uint32_t version, std::string type,
                                                                  std::optional<std::string> hardware) override;

  // See IFirmwareChecker
  void checkNow(const std::string device_type,
                const std::optional<std::string> device_hardware = std::nullopt) override;

private:
  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_log_level_t log_level);

  static void run_task(void *pvParams);

  void checkFirmware();
  void checkFirmware(FirmwareDevice &device);

private:
  struct Firmware {
    uint32_t version;
    std::string md5;
  };

  std::string _base_url;
  std::vector<OnLog> _on_log;
  Configuration _configuration;
  EventGroupHandle_t _check_now_event_group;
  std::set<FirmwareDevice> _available_devices;
  std::vector<OnAvailableFirmware> _on_available_firmware;
  std::optional<FirmwareDevice> _device_to_check_now = std::nullopt;

  unsigned long _checked_device_last_at_ms = 0;
  std::set<FirmwareDevice>::iterator _devices_iterator = _available_devices.end();
  std::map<FirmwareDevice, Firmware> _firmware_version_for_device;
};

#endif // __FIRMWARE_CHECKER_H__