#ifndef __IFIRMWARE_CHECKER_H__
#define __IFIRMWARE_CHECKER_H__

#include <cstdint>
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
};

#endif // __IFIRMWARE_CHECKER_H__