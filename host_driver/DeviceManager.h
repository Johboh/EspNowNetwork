#ifndef __DEVICE_MANAGER_H__
#define __DEVICE_MANAGER_H__

#include "IDeviceManager.h"
#include "esp_log.h"
#include <Device.h>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

/**
 * @brief Called by host/router when a new message has been received over ESP-NOW.
 * This class check MAC address and forward the messages to respective specific implementation.
 */
class DeviceManager : public IDeviceManager {
public:
  /**
   * @brief Callback when the device manager want to log something.
   *
   * This doesn't need to be implemented. But can be used to print debug information to serial
   * or post debug information to for example an MQTT topic.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  using OnLog = std::function<void(const std::string message, const esp_log_level_t log_level)>;

  using IsConnected = std::function<bool(void)>;

  /**
   * @brief Construct a new Device Manager object
   *
   * @param devices the list of devices.
   * @param is_connected return if we are currently connected to MQTT.
   */
  DeviceManager(std::vector<std::reference_wrapper<Device>> &devices, IsConnected is_connected);

public:
  /**
   * @brief Should be called from main loop
   */
  void handle();

  /**
   * @brief Set callback to use when the device manager want to log something.
   * Only supports one callback. If a new callback is set, the old one will be overwritten.
   * Set to {} to disable logging.
   */
  void setOnLog(OnLog on_log) { _on_log = on_log; }

  /**
   * @brief Forward a message to a device.
   *
   * @param retries number of retries before this packet was received, as indicated by the node.
   * @param mac_address MAC address of sender.
   * @param message the message to forward.
   */
  void forward(uint8_t retries, uint64_t mac_address, const uint8_t *message) override;

  /**
   * @brief Given a MAC address, return a device.
   * return std::nullopt if no device was found.
   */
  std::optional<std::reference_wrapper<Device>> deviceForMac(uint64_t mac_address) override;

private:
  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_log_level_t log_level);

private:
  bool _was_connected = false;

private:
  OnLog _on_log;
  IsConnected _is_connected;

private:
  std::map<uint64_t, unsigned long> _last_message_ms;
  std::map<uint64_t, std::reference_wrapper<Device>> _devices;
};

#endif // __DEVICE_MANAGER_H__