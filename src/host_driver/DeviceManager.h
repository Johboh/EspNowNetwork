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

#define DEFAULT_STACK_SIZE (4096)
#define DEFAULT_TASK_PRIORITY (10)

/**
 * @brief Called by host/router when a new message has been received over ESP-NOW.
 * This class check MAC address and forward the messages to respective specific implementation.
 */
class DeviceManager : public IDeviceManager {
public:
  using IsConnected = std::function<bool(void)>;

  struct Configuration {
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

  inline static Configuration _default = {.task_size = DEFAULT_STACK_SIZE, .task_priority = DEFAULT_TASK_PRIORITY};

  /**
   * @brief Construct a new Device Manager object
   *
   * @param devices the list of devices.
   * @param is_connected return if we are currently connected to MQTT.
   */
  DeviceManager(std::vector<std::reference_wrapper<Device>> &devices, IsConnected is_connected,
                Configuration configuration = _default);

public:
  /**
   * @brief Must be called once an internet connection has been established and ESP NOW has been setup.
   */
  void start();

  /**
   * @brief Call to drive the Device Manager. Only needed if task_size in configuration is set to 0. Otherwise, this
   * is done by the Device Manager task.
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
  Configuration _configuration;

private:
  std::map<uint64_t, unsigned long> _last_message_ms;
  std::map<uint64_t, std::reference_wrapper<Device>> _devices;
};

#endif // __DEVICE_MANAGER_H__