#ifndef __IFIRMWARE_CHECKER_H__
#define __IFIRMWARE_CHECKER_H__

#include <cstdint>
#include <esp_log.h>
#include <functional>
#include <optional>
#include <string>

/**
 * @brief Given a firmware version, a type and an optional hardware, return if there is any new firmware available.
 */
class IFirmwareChecker {
public:
  struct UpdateInformation {
    std::string url;  // Firmware version where binary can be found.
    std::string md5;  // MD5 checksum of the firmware binary.
    uint32_t version; // The new firmware version.
  };

  /**
   * @brief Given a firmare version, get the update URI and md5 to use to update this device to a newer version.
   * Optional absent if there is no newer firmware available.
   *
   * @param version the current firmware version of the device.
   * @param type the type to get firmware version for (one of devices())
   * @param hardware the hardware to get firmware version for (one of devices())
   * @return the UpdateInformation with firmware URL, MD5 to use to update if there is a newer version available,
   * else std::nullopt if there is no newer version.
   */
  virtual std::optional<UpdateInformation> getUpdateUrl(uint32_t version, std::string type,
                                                        std::optional<std::string> hardware) = 0;

  /**
   * @brief Callback when the firmware checker want to log something.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  using OnLog = std::function<void(const std::string message, const esp_log_level_t log_level)>;

  /**
   * @brief Called by the HostDriver to add a logger callback for the firmware checker. The host driver will
   * use this to log messages on MQTT.
   * If the firmware checker doesn't provide any logs, this can be omitted.
   */
  virtual void addOnLog(OnLog on_log) {}

  /**
   * @brief callback when a firmware version has been downloaded from the server.
   * This indicates that there is a firmware available, not nessesary that it is the latest or newer than any current
   * firwmare that devices are using. The version has to be compared to the current version of the target node.
   *
   * @param device_type the device type.
   * @param device_hardware the device hardware, optional.
   * @param firmware_version the firmware version.
   * @param firmware_md5 the firmware md5.
   */
  using OnAvailableFirmware =
      std::function<void(const std::string device_type, const std::optional<std::string> device_hardware,
                         const uint32_t firmware_version, const std::string firmware_md5)>;

  /**
   * @brief Called by the HostDriver to add a callback when a firmware version has been found on a server.
   * The host driver will use this to log messages on MQTT on available firmware versions per type and hardware.
   * Can be omitted.
   */
  virtual void addOnAvailableFirmware(OnAvailableFirmware on_available_firmware) {}

  /**
   * @brief Call to force a check of the firmware for the given type and optional hardware.
   * Once firmware have been fetched, the OnAvailableFirmware callback will be invoked.
   *
   * @param device_type the device type.
   * @param device_hardware the device hardware, optional.
   */
  virtual void checkNow(const std::string device_type,
                        const std::optional<std::string> device_hardware = std::nullopt) {}
};

#endif // __IFIRMWARE_CHECKER_H__