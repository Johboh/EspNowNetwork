#ifndef __IDEVICE_MANAGER_H__
#define __IDEVICE_MANAGER_H__

#include <Device.h>
#include <cstdint>
#include <esp_log.h>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * @brief Called by host/router when a new message has been received over ESP-NOW.
 * This class check MAC address and forward the messages to respective specific implementation.
 */
class IDeviceManager {
public:
  /**
   * @brief Forward a message to a device.
   *
   * @param retries number of retries before this packet was received, as indicated by the node.
   * @param mac_address MAC address of sender.
   * @param message the message to forward.
   */
  virtual void forward(uint8_t retries, uint64_t mac_address, const uint8_t *message) = 0;

  /**
   * @brief Given a MAC address, return a device.
   * return std::nullopt if no device was found.
   */
  virtual std::optional<std::reference_wrapper<Device>> deviceForMac(uint64_t mac_address) = 0;

  /**
   * @brief Callback when the device manager want to log something.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  using OnLog = std::function<void(const std::string message, const esp_log_level_t log_level)>;

  /**
   * @brief Called by the HostDriver to add a logger callback for the device manager. The host driver will
   * use this to log messages on MQTT.
   * If the device manager doesn't provide any logs, this can be omitted.
   */
  virtual void addOnLog(OnLog on_log) {}
};

#endif // __IDEVICE_MANAGER_H__