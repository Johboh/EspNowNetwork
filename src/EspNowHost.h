#ifndef __ESP_NOW_HOST_H__
#define __ESP_NOW_HOST_H__

#include "esp_log.h"
#include <Arduino.h>
#include <EspNowCrypt.h>
#include <functional>
#include <map>

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
  typedef std::function<void(const String message, const esp_log_level_t log_level)> OnLog;

  typedef std::function<void(void)> OnNewMessage;

  /**
   * @brief Callback on valid application message received.
   *
   * @param metadata metadata for the message.
   * @param message the received decrypted, validated application message.
   */
  typedef std::function<void(MessageMetadata metadata, const uint8_t *message)> OnApplicationMessage;

public:
  /**
   * @brief Construct a new EspNowHost
   *
   * @param crypt the EspNowCrypt to use for encrypting/decrypting messages.
   * @param on_new_message callback on any new message received, regardless of type, validation, decrypted correctly
   * etc. Intended for turning on led or similar to indicate new package.
   * @param on_application_message callback when there is a verified, decrypted application message received.
   * @param on_log callback when the host want to log something.
   */
  EspNowHost(EspNowCrypt &crypt, OnNewMessage on_new_message, OnApplicationMessage on_application_message,
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
  void handleDiscoveryRequest(uint8_t *mac_addr);
  void handleChallengeRequest(uint8_t *mac_addr);

  void sendMessageToTemporaryPeer(uint8_t *mac_addr, void *message, size_t length);

  uint64_t macToMac(uint8_t *mac_addr);

  void log(const String message, const esp_log_level_t log_level);

private:
  EspNowCrypt &_crypt;
  // Map from MAC address to challenge.
  std::map<uint64_t, uint32_t> _challenges;

  OnLog _on_log;
  OnNewMessage _on_new_message;
  OnApplicationMessage _on_application_message;
};

#endif // __ESP_NOW_HOST_H__