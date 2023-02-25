#ifndef __ESP_NOW_NODE_H__
#define __ESP_NOW_NODE_H__

#include "esp_log.h"
#include <Arduino.h>
#include <EspNowCrypt.h>
#include <Preferences.h>

#define NUM_RETRIES 50

/**
 * @brief ESP Now Network: Node
 *
 * This is the node engine for the EspNowNetwork and works together with the host running the EspNowHost engine.
 * The node is intended to be a sensor type kind of node that sends messages every now and then. It can preferably run
 * on battery and be in sleep/deep sleep most of the time.
 *
 * The Node part of the EspNowNetwork supports the following:
 * - Setting up ESP-NOW: setup().
 * - Sending discovery requests and listen for replies (for the node to disover the host).
 * - Sending challenge request and listen for replies (for encryption reply attacks protection).
 * - Sending the application message.
 *
 * Check the main README.md file for the full EspNowNetwork overview.
 */
class EspNowNode {
public:
  /**
   * @brief Callback when the node want to log something.
   *
   * This doesn't need to be implemented. But can be used to print debug information to serial.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  typedef std::function<void(const String message, const esp_log_level_t log_level)> OnLog;

  /**
   * @brief Construct a new EspNowNode.
   *
   * @param crypt the EspNowCrypt to use for encrypting/decrypting messages.
   * @param on_log callback when the host want to log something.
   */
  EspNowNode(EspNowCrypt &crypt, OnLog on_log = {});

public:
  /**
   * @brief Setup the ESP-NOW stack
   *
   * If there is already a known host (MAC address) stored in Preferences/Flash, this MAC address will be used
   * in the sendMessage() call.
   * If there is no stored MAC address, a discovery will start.
   * After the ESP-NOW is setup, a broadcast disovery request message is sent. A EspNowHost device will reply to this.
   * Upon reply, the MAC address will be persisted to Preferences/Flash, and the device will reboot.
   * If there is no valid reply (after a certain number of retries to discover a host), this method will return false.
   */
  bool setup();

  /**
   * @brief Send a message to the host (see setup()). Can only be called after a successful setup().
   *
   * Before the application message is sent, there will be a challenge request/response message exhange with the host.
   *
   * @param message the message to send.
   * @param message_size the size of the message.
   * @param retries number of times to retry on delivery failure. This function
   * will block until successful or failing delivery of the message. If set to -1,
   * it will only try once.
   */
  bool sendMessage(void *message, size_t message_size, int16_t retries = NUM_RETRIES);

  /**
   * Calling this will clear the host.
   * This will clear the stored host MAC, so a new discovery is needed.
   * This will also disable sendMessage(), so a new setup() call is needed after this.
   */
  void forgetHost();

private:
  void sendMessageInternal(uint8_t *buff, size_t length);

  /**
   * @brief Send a message and wait for a response message.
   * Returns a pointer to the decrypted received message, or nullptr if no message received within the timout, or if the
   * decryption failed.
   *
   * @param message message to send.
   * @param length length of message to send.
   * @param out_mac_addr the MAC address of the send of the received message. Must be of size ESP_NOW_ETH_ALEN. If null,
   * will not populate.
   */
  std::unique_ptr<uint8_t[]> sendAndWait(uint8_t *message, size_t length, uint8_t *out_mac_addr = nullptr);

private:
  OnLog _on_log;
  EspNowCrypt &_crypt;
  Preferences _preferences;
  bool _setup_successful = false;
  uint8_t _esp_now_host_address[6];
};

#endif // __ESP_NOW_NODE_H__