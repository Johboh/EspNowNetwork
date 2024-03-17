#ifndef __FIRMWARE_CHECKER_H__
#define __FIRMWARE_CHECKER_H__

#include "IFirmwareChecker.h"
#include "esp_log.h"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

using OnAvailableFirmware = IFirmwareChecker::OnAvailableFirmware;
using OnLog = IFirmwareChecker::OnLog;

#define DEFAULT_CHECK_EACH_TYPE_EVERY_MS (30000L) // Check one type every 30s
#define DEFAULT_STACK_SIZE (4096)
#define DEFAULT_TASK_PRIORITY (10)

/**
 * @brief Periodically fetch the latest available firmware version.
 */
class FirmwareChecker : public IFirmwareChecker {
public:
  struct Device {
    std::string type;
    std::optional<std::string> hardware;

    bool operator==(const Device &other) const { return type == other.type && hardware == other.hardware; }
    bool operator<(const Device &other) const {
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
    unsigned long check_every_ms = DEFAULT_CHECK_EACH_TYPE_EVERY_MS;

    /**
     * @brief if set to non 0, will create task for driving the Device Manager. With this, there is no need to call the
     * handle() function. Set to 0 to manually call the handle() in your own main loop.
     */
    unsigned long task_size = DEFAULT_STACK_SIZE;

    /**
     * @brief Priority for driving task. Only used if task_size is non zero.
     *
     */
    uint8_t task_priority = DEFAULT_TASK_PRIORITY;
  };

  inline static Configuration _default = {.check_every_ms = DEFAULT_CHECK_EACH_TYPE_EVERY_MS,
                                          .task_size = DEFAULT_STACK_SIZE,
                                          .task_priority = DEFAULT_TASK_PRIORITY};

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
  FirmwareChecker(std::string base_url, const std::set<Device> &devices, Configuration configuration = _default);

public:
  /**
   * @brief Must be called once an internet connection has been established.
   */
  void start();

  /**
   * @brief Call to drive the Firmware Checker. Only needed if task_size in configuration is set to 0. Otherwise, this
   * is done by the Firmware Checker task.
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

private:
  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_log_level_t log_level);

  static void run_task(void *pvParams);

private:
  struct Firmware {
    uint32_t version;
    std::string md5;
  };

  std::string _base_url;
  std::vector<OnLog> _on_log;
  Configuration _configuration;
  const std::set<Device> &_available_devices;
  std::vector<OnAvailableFirmware> _on_available_firmware;

  unsigned long _checked_device_last_at_ms = 0;
  std::set<Device>::iterator _devices_iterator = _available_devices.end();
  std::map<Device, Firmware> _firmware_version_for_device;
};

#endif // __FIRMWARE_CHECKER_H__