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

using OnLog = IDeviceManager::OnLog;

namespace DeviceManagerDefaults {
const uint32_t DEFAULT_STACK_SIZE = 4096;
const uint32_t DEFAULT_TASK_PRIORITY = 7;
} // namespace DeviceManagerDefaults

/**
 * @brief Called by host/router when a new message has been received over ESP-NOW.
 * This class check MAC address and forward the messages to respective specific implementation.
 */
class DeviceManager : public IDeviceManager {
public:
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
   * @brief Start a task that will drive the Device Manager. By calling this function, there is no
   * need to manually call the handle() function.
   */
  void startTask(unsigned long task_size = DeviceManagerDefaults::DEFAULT_STACK_SIZE,
                 uint8_t task_priority = DeviceManagerDefaults::DEFAULT_TASK_PRIORITY);

  /**
   * @brief Manually drive the Device Manager. Must be called perodically if startTask() was not called.
   */
  void handle();

  // See IDeviceManager
  void addOnLog(OnLog on_log) override { _on_log.push_back(on_log); }

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

  static void run_task(void *pvParams);

private:
  bool _was_connected = false;

private:
  IsConnected _is_connected;
  std::vector<OnLog> _on_log;

private:
  std::map<uint64_t, unsigned long> _last_message_ms;
  std::map<uint64_t, std::reference_wrapper<Device>> _devices;
};

#endif // __DEVICE_MANAGER_H__