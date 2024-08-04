#ifndef __ESP_NOW_NODE_H__
#define __ESP_NOW_NODE_H__

#include "EspNowOta.h"
#include "Preferences.h"
#include <EspNowCrypt.h>
#include <esp_idf_version.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <functional>
#include <string>

#define NUM_MESSAGE_RETRIES 50

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
  typedef std::function<void(const std::string message, const esp_log_level_t log_level)> OnLog;

  enum class Status {
    /**
     * @brief We don't know about the MAC host address and/or WiFi chanel, so starting the disovery process to find the
     * host MAC/WiFi channel.
     */
    HOST_DISCOVERY_STARTED,

    /**
     * @brief The host MAC and WiFi channel was found.
     */
    HOST_DISCOVERY_SUCCESSFUL,

    /**
     * @brief Unable to find the host MAC and/or WiFi channel. This is most probably due to the host being offline.
     */
    HOST_DISCOVERY_FAILED,

    /**
     * @brief Host failed to acknowledge messages when trying to send a message. The persisted host is most probably
     * invalid. The host has now been forgotten, and a new setup is needed.
     */
    INVALID_HOST,

    /**
     * @brief The host indicated that a firmware update is needed, and such, a firmware update has started.
     * This will follow by a FIRMWARE_UPDATE_SUCCESSFUL or FIRMWARE_UPDATE_FAILED/FIRMWARE_UPDATE_WIFI_SETUP_FAILED.
     */
    FIRMWARE_UPDATE_STARTED,

    /**
     * @brief Firmware update succeeded. The device will be restarted (using esp_restart()).
     * TODO(johboh): Consider making the restart optional?
     */
    FIRMWARE_UPDATE_SUCCESSFUL,

    /**
     * @brief Firmware update failed. The device will be restarted (using esp_restart()).
     * TODO(johboh): Consider making the restart optional?
     */
    FIRMWARE_UPDATE_FAILED,

    /**
     * @brief Firmware update failed as was unable to setup WiFi. The device will be restarted (using esp_restart()).
     * TODO(johboh): Consider making the restart optional?
     */
    FIRMWARE_UPDATE_WIFI_SETUP_FAILED,
  };

  /**
   * @brief Callback on status changes. See Status enum on the different statuses available and suggestion on how to
   * handle them.
   *
   * @param status the new current status.
   */
  typedef std::function<void(const Status status)> OnStatus;

  /**
   * @brief CRT Bundle Attach for Ardunio or ESP-IDF from MDTLS, to support TLS/HTTPS firmware URIs.
   *
   * Include esp_crt_bundle.h and pass the following when using respective framework:
   * for Arduino: arduino_esp_crt_bundle_attach
   * for ESP-IDF: esp_crt_bundle_attach
   *
   * C style function.
   */
  typedef EspNowOta::CrtBundleAttach CrtBundleAttach;

  /**
   * @brief Construct a new EspNowNode.
   *
   * @param crypt the EspNowCrypt to use for encrypting/decrypting messages.
   * @param preferences the EspNowNetwork::Preferences to use for storing/reading preferences.
   * @param firmware_version the (incremental) firmware version that this node is currently running.
   * @param on_status callback on status changes. See Status enum on the different statuses available and suggestion on
   * how to handle them.
   * @param on_log callback when the host want to log something.
   * @param crt_bundle_attach crt_bundle_attach for either Ardunio (arduino_esp_crt_bundle_attach) or ESP-IDF
   * (esp_crt_bundle_attach).
   */
  EspNowNode(EspNowCrypt &crypt, EspNowNetwork::Preferences &preferences, uint32_t firmware_version,
             OnStatus on_status = {}, OnLog on_log = {}, CrtBundleAttach crt_bundle_attach = nullptr);

public:
  /**
   * @brief Setup the ESP-NOW stack
   *
   * If there is already a known host (MAC address) and Wifi channel stored in Preferences/Flash, this MAC address and
   * channel will be used in the sendMessage() call. If there is no stored MAC address or valid WiFi channel, a
   * discovery will start. After the ESP-NOW is setup, a broadcast disovery request message is sent. A EspNowHost device
   * will reply to this. Upon reply, the MAC address and WiFi channel will be persisted to Preferences/Flash. If there
   * is no valid reply (after a certain number of retries to discover a host), this method will return false.
   *
   * Note that as ESP-NOW depend on WiFi, the EspNowNode will not work togheter with WiFi. It assumes no WiFi is
   * previosly setup or will be setup. A node is supposed to use Esp-NOW only as means of communication.
   *
   * Must be called before fist call to sendMessage().
   *
   * @return true on sucessful setup, or false if failed to setup ESP-NOW or failed to do discovery.
   */
  bool setup();

  /**
   * @brief Tear down any esp now/wifi setup. This will invalidate the state and another setup call is needed afte this.
   * Useful to call before any kind of sleep or similar.
   * Note that as ESP-NOW rely on wifi, this will also stop any WiFi. However, nodes should not have WiFi and ESP-NOW at
   * the same time to begin with.
   */
  void teardown();

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
  bool sendMessage(void *message, size_t message_size, int16_t retries = NUM_MESSAGE_RETRIES);

  /**
   * Calling this will clear the host.
   * This will clear the stored host MAC, so a new discovery is needed.
   * This will also disable sendMessage(), so a new setup() call is needed after this.
   */
  void forgetHost();

private:
  static void esp_now_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
  static void esp_now_on_data_callback_legacy(const uint8_t *mac_addr, const uint8_t *data, int data_len);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  static void esp_now_on_data_callback(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len);
#endif

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

  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_log_level_t log_level);

  /**
   * @brief Log if log callback is available.
   */
  void log(const std::string message, const esp_err_t esp_err);

  /**
   * @brief Connects to WiFi and download new firmware.
   *
   * Will never return. Will restart on success or on failure.
   */
  void handleFirmwareUpdate(char *wifi_ssid, char *wifi_password, char *url, char *md5);

  bool isValidWiFiChannel(uint8_t channel);
  bool isValidWiFiChannel(std::optional<uint8_t> &channel_opt);

private:
  OnLog _on_log;
  OnStatus _on_status;
  EspNowCrypt &_crypt;
  esp_netif_t *_netif_sta;
  uint32_t _firmware_version;
  bool _setup_successful = false;
  bool _esp_now_initialized = false;
  CrtBundleAttach _crt_bundle_attach;
  esp_now_peer_info_t _host_peer_info;
  EspNowNetwork::Preferences &_preferences;
};

#endif // __ESP_NOW_NODE_H__