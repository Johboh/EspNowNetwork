#ifndef __ESP_NOW_HOST_H__
#define __ESP_NOW_HOST_H__

#include <EspNowCrypt.h>
#include <esp_log.h>
#include <functional>
#include <map>
#include <optional>
#include <string>

/**
 * @brief ESP Now Network: Host
 *
 * This is the host engine for the EspNowNetwork and works together with nodes running the EspNowNode engines.
 * The host is intended to always be up and running to listen for messages from the nodes.
 * A common setup is that the host is also connecting to WiFi and forward all incoming messages to for example MQTT.
 *
 * The Host part of the EspNowNetwork supports the following:
 * - Setting up ESP-NOW: setup().
 * - Listen for (and responds to) discovery requests from nodes (for nodes to disover the host).
 * - Listen for (and responds to) challenge requests from nodes (for encryption reply attacks protection).
 * - forwards all incoming application messages in the lambda callback.
 *
 * Check the main README.md file for the full EspNowNetwork overview.
 */
class EspNowHost {
public:
  struct MessageMetadata {
    // How many times the sender tried to send the message until it was received by the host.
    // useful to identify nodes with poor connection.
    uint8_t retries = 0;
    uint64_t mac_address; // MAC address of the sender node as an uint64_t
  };

  /**
   * @brief Callback when the host want to log something.
   *
   * This doesn't need to be implemented. But can be used to print debug information to serial
   * or post debug information to for example an MQTT topic.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  typedef std::function<void(const std::string message, const esp_log_level_t log_level)> OnLog;

  typedef std::function<void(void)> OnNewMessage;

  /**
   * @brief Callback on valid application message received.
   *
   * @param metadata metadata for the message.
   * @param message the received decrypted, validated application message.
   */
  typedef std::function<void(MessageMetadata metadata, const uint8_t *message)> OnApplicationMessage;

  struct FirmwareUpdate {
    char wifi_ssid[32];     // WiFi SSID that node should connect to.
    char wifi_password[32]; // WiFi password that the node should connect to.
    char url[96];           // url where to find firmware binary. Note the max file path.
    char md5[32];           // MD5 hash of firmware. Does not include trailing \0
  };

  /**
   * @brief Callback that if returning a [FirmwareUpdate], will tell host to upgrade its firmware given the metadata in
   * [FirmwareUpdate].
   *
   * @param mac_address the MAC address of the node.
   * @param firmware_version firmware version as reported by the node.
   * return std::nullopt if there is no firmware update available for the given mac_address
   */
  typedef std::function<std::optional<FirmwareUpdate>(uint64_t mac_address, uint32_t firmware_version)>
      FirmwareUpdateAvailable;

  enum class WiFiInterface {
    AP,  // Use the Access Point interface for ESP-NOW.
    STA, // Use the Station/Client interface for ESP-NOW.
  };

public:
  /**
   * @brief Construct a new EspNowHost
   *
   * @param crypt the EspNowCrypt to use for encrypting/decrypting messages.
   * @param wifi_interface what network interface to use to send ESP-NOW messages on. You need to setup this interface
   * beforehand when setting up your WiFi.
   * @param on_new_message callback on any new message received, regardless of type, validation, decrypted correctly
   * etc. Intended for turning on led or similar to indicate new package.
   * @param on_application_message callback when there is a verified, decrypted application message received.
   * @param firwmare_update callback to check if a firmware update is available. Please note that this function will be
   * called on every challenge request sent by the node, so this function must return fast and not perform any heavy
   * computation or network. This function should preferably just do a lookup in a lookup table to check if a given node
   * and its firmware version have new firmware.
   * @param on_log callback when the host want to log something.
   */
  EspNowHost(EspNowCrypt &crypt, WiFiInterface wifi_interface, OnNewMessage on_new_message,
             OnApplicationMessage on_application_message, FirmwareUpdateAvailable firwmare_update = {},
             OnLog on_log = {});

public:
  /**
   * @brief Setup the ESP-NOW stack. Preferably called from Arduino main setup() function.
   */
  bool setup();

  /**
   * @brief Call to process queue and others. Should be called from Arduino main loop()
   */
  void handle();

private:
  void handleQueuedMessage(uint8_t *mac_addr, uint8_t *data);
  void handleDiscoveryRequest(uint8_t *mac_addr, uint32_t discovery_challenge);
  void handleChallengeRequest(uint8_t *mac_addr, uint32_t challenge_challenge, uint32_t firmware_version);

  void sendMessageToTemporaryPeer(uint8_t *mac_addr, void *message, size_t length);

  uint64_t macToMac(uint8_t *mac_addr);

  void log(const std::string message, const esp_log_level_t log_level);
  void log(const std::string message, const esp_err_t esp_err);

  std::string toHex(uint64_t i);

private:
  EspNowCrypt &_crypt;
  WiFiInterface _wifi_interface;
  // Map from MAC address to challenge.
  std::map<uint64_t, uint32_t> _challenges;

  OnLog _on_log;
  OnNewMessage _on_new_message;
  FirmwareUpdateAvailable _firwmare_update;
  OnApplicationMessage _on_application_message;
};

#endif // __ESP_NOW_HOST_H__