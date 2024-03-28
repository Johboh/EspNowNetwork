#ifndef __I_FIRMWARE_KICKER_H__
#define __I_FIRMWARE_KICKER_H__

#include <cstdint>
#include <esp_err.h>

/**
 * @brief An implemetantion that triggers a firmware check.
 */
class IFirmwareKicker {
public:
  /**
   * @brief Callback when the kicker server want to log something.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  using OnLog = std::function<void(const std::string message, const esp_log_level_t log_level)>;

  /**
   * @brief Called by the HostDriver to add a logger callback for the kicker server. The host driver will
   * use this to log messages on MQTT.
   * If the kicker server doesn't provide any logs, this can be omitted.
   */
  virtual void addOnLog(OnLog on_log) {}
};

#endif // __I_FIRMWARE_KICKER_H__