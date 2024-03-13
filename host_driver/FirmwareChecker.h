#ifndef __FIRMWARE_CHECKER_H__
#define __FIRMWARE_CHECKER_H__

#include "IFirmwareChecker.h"
#include "esp_log.h"
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>

#define DEFAULT_CHECK_EACH_TYPE_EVERY_MS (30000L) // Check one type every 30s

/**
 * @brief Periodically fetch the latest available firmware version.
 */
class FirmwareChecker : public IFirmwareChecker {
public:
  /**
   * @brief Callback when the firmware checker want to log something.
   *
   * This doesn't need to be implemented. But can be used to print debug information to serial
   * or post debug information to for example an MQTT topic.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  using OnLog = std::function<void(const std::string message, const esp_log_level_t log_level)>;

  /**
   * @brief callback when a firmware version has been downloaded from the server.
   *
   * @param device_type the device type.
   * @param device_hardware the device hardware, optional.
   * @param firmware_version the firmware version.
   * @param firmware_md5 the firmware md5.
   */
  using OnAvailableFirmware =
      std::function<void(const std::string device_type, const std::optional<std::string> device_hardware,
                         const uint32_t firmware_version, const std::string firmware_md5)>;

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

  /**
   * @brief Construct a new Firmware Checker object
   *
   * @param base_url Base address on where to find firmware binary, version and optional MD5 hash. must end with a
   * trailing /. The device type and hardware (if present) will be appended as path to this path.
   *
   * The following must exist:
   * firmware.bin - the binary itself.
   * firmware_version.txt - containing the firmware version as a number
   * The following is optional:
   * firmware.md5 - the MD5 hash of the binary, as 32 hex characters
   *
   * Example:
   * if the base_url is http://192.168.1.128/ the device type is "motion" and hardware is "lolin_c2",
   * then the final URL for getting firmware version will be http://192.168.1.128/motion/lolin_c2/firmware_version.txt
   * and the binary will be http://192.168.1.128/motion/lolin_c2/firmware.bin
   * The URL to the binary is what will be send to the node on OTA update.
   * If there is no hardware, the hardware part will be omitted and the final URL will be
   * http://192.168.1.128/motion/firmware_version.txt will be appended to this url, followed by firmware_version.txt.
   * Example: http://192.168.1.128/. If device type is
   *
   * It will also try to download firmware.md5. If found, will include MD5 hash of firmware when doing OTA to verify the
   * binary, but its optional.
   *
   * Note! No support for HTTPS as of now.
   *
   * @param devices all available type and (optional) hardware combinations.
   * @param check_every_ms how often to check one type and hardware combination, in milliseconds.
   */
  FirmwareChecker(std::string base_url, const std::set<Device> &devices,
                  unsigned long check_every_ms = DEFAULT_CHECK_EACH_TYPE_EVERY_MS);

public:
  /**
   * @brief Should be called from main loop
   */
  void handle();

  /**
   * @brief Set callback to use when the firmware checker want to log something.
   * Only supports one callback. If a new callback is set, the old one will be overwritten.
   * Set to {} to disable logging.
   */
  void setOnLog(OnLog on_log = {}) { _on_log = on_log; }

  /**
   * @brief Set callback to use when a firmware version has been downloaded from the server.
   * Only supports one callback. If a new callback is set, the old one will be overwritten.
   * Set to {} to disable.
   */
  void setOnAvailableFirmware(OnAvailableFirmware on_available_firmware = {}) {
    _on_available_firmware = on_available_firmware;
  }

  /**
   * @brief Given a firmare version, get the update URI and md5 to use to update this device to a newer version.
   * Optional absent if there is no newer firmware available.
   *
   * @param version the current firmware version of the device.
   * @param type the type to get firmware version for (one of devices())
   * @param hardware the hardware to get firmware version for (one of devices())
   * @return the firmware URL and md5 to use to update if there is a newer version available, else std::nullopt
   */
  std::optional<IFirmwareChecker::UpdateInformation> getUpdateUrl(uint32_t version, std::string type,
                                                                  std::optional<std::string> hardware) override;

private:
  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_log_level_t log_level);

private:
  struct Firmware {
    uint32_t version;
    std::string md5;
  };

  OnLog _on_log;
  std::string _base_url;
  unsigned long _check_every_ms;
  OnAvailableFirmware _on_available_firmware;
  const std::set<Device> &_available_devices;

  unsigned long _checked_device_last_at_ms = 0;
  std::set<Device>::iterator _devices_iterator = _available_devices.end();
  std::map<Device, Firmware> _firmware_version_for_device;
};

#endif // __FIRMWARE_CHECKER_H__