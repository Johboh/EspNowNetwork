#ifndef __HOST_DRIVER_H__
#define __HOST_DRIVER_H__

#include "esp_log.h"
#include <EspNowCrypt.h>
#include <EspNowHost.h>
#include <FirmwareChecker.h>
#include <IDeviceManager.h>
#include <IFirmwareChecker.h>
#include <cstdint>
#include <driver/gpio.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Wrapper around all of the host logic, to make main.cpp cleaner.
 */
class HostDriver {
public:
  /**
   * @brief Callback when the firmware checker want to log something.
   *
   * This doesn't need to be implemented. Can be used to print debug information to serial
   * or post debug information to for example an MQTT topic.
   *
   * @param message the message to log.
   * @param sub_path This will be a root path starting with /, and is expected to be added to after a MQTT client ID of
   * some sort. For example, mqtt_client_id + sub_path. If logging to Serial, this can be omitted.
   * @param retain If true, the message should be retained in MQTT. If logging to Serial, this can be omitted.
   */
  using OnLog = std::function<void(const std::string message, const std::string sub_path, const bool retain)>;

  /**
   * @brief Callback on new message recieved.
   *
   * This doesn't need to be implemented. Can be used to print debug information or turn on a led upon new message.
   *
   */
  using OnMessage = std::function<void(void)>;

  struct Configuration {
    /**
     * @brief The SSID for the WiFi network that nodes should use when performing firmware update.
     */
    const char *wifi_ssid;
    /**
     * @brief The password for the WiFi network that nodes should use when performing firmware update.
     */
    const char *wifi_password;
    /**
     * @brief Encyption key used for our packet encryption (GCM). Must be exact 16 bytes long. \0
     * does not count.
     */
    const char *esp_now_encryption_key;
    /**
     * @brief Used to validate the integrity of the messages. We expect the decrypted payload to
     * contain this string. Must be exact 8 bytes long. \0 does not count.
     */
    const char *esp_now_encryption_secret;
    /**
     * @brief Host configuration.
     */
    EspNowHost::Configuration host_configuration = {};
  };

  /**
   * @brief Construct a new Host Driver object
   *
   * @param device_manager the Device Manager to use.
   * @param configuration the configuration to use. See [Configuration] struct.
   * @param on_log Callback when the device manager want to log something.
   * @param on_message Callback on new message recieved.
   */
  HostDriver(IDeviceManager &device_manager, Configuration configuration, OnLog on_log = {}, OnMessage on_message = {});

public:
  /**
   * @brief Call to setup the host.
   *
   * @param firmware_checker the Firmware Checker to use. Optional.
   */
  void setup(std::optional<std::reference_wrapper<IFirmwareChecker>> firmware_checker = std::nullopt);

private:
  std::string logLevelToString(const esp_log_level_t log_level);

private:
  void onNewMessage();
  void onHostLog(const std::string message, const esp_log_level_t log_level);
  void onFirwmareLog(const std::string message, const esp_log_level_t log_level);
  void onAvailableFirwmare(const std::string device_type, const std::optional<std::string> device_hardware,
                           const uint32_t firmware_version, const std::string firmware_md5);
  void onDeviceManagerLog(const std::string message, const esp_log_level_t log_level);

  std::optional<EspNowHost::FirmwareUpdate> onFirmwareUpdate(uint64_t mac_address, uint32_t firmware_version,
                                                             const char *wifi_ssid, const char *wifi_password);
  void onNewApplicationMessage(EspNowHost::MessageMetadata metadata, const uint8_t *message);

  std::string makeMqttPathCompatible(const std::string &input);

  void log(const std::string sub_path, const std::string message, const bool retain = false);

private:
  OnLog _on_log;
  OnMessage _on_message;
  EspNowCrypt _esp_now_crypt;
  EspNowHost _esp_now_host;
  IDeviceManager &_device_manager;
  std::optional<std::reference_wrapper<IFirmwareChecker>> _firmware_checker;

private:
  unsigned long _log_messages = 0;
};

#endif // __HOST_DRIVER_H__