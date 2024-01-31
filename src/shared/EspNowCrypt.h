#ifndef __ESP_CRYPT_H__
#define __ESP_CRYPT_H__

#include <esp_err.h>
#include <memory>

/**
 * @brief ESP Now Network: Send encrypted messages on ESP NOW and decrypt incoming messages.
 *
 * Check the main README.md file for the full EspNowNetwork overview.
 */
class EspNowCrypt {
public:
  /**
   * @brief Construct a new Esp Crypt object
   *
   * @param key Encyption key used for our own packet encryption (GCM). Must be exact 16 bytes long. \0 does not count.
   * @param secret Used to validate the integrity of the messages. We expect the decrypted payload to contain this
   * string. Must be exact 8 bytes long. \0 does not count.
   */
  EspNowCrypt(const char *key, const char *secret);

public:
  /**
   * @brief Send a message on ESP-NOW.
   * The message to send should be the application message. This message will be encryped and
   * placed after the Encryption Header.
   *
   * @param mac_addr the receiver of the message (unicast or broadcast). Should be ESP_NOW_ETH_ALEN in length.
   * @param message pointer to the message to send.
   * @param length size of the message.
   * @return esp_err_t result of the send attempt. ESP_OK if all good.
   */
  esp_err_t sendMessage(const uint8_t *mac_addr, const void *message, const size_t length);

  /**
   * @brief Decrypt a recived message. This message is assumed to contain the Encryption Header.
   *
   * @param message the message to decrypt.
   * @return a non null pointer to the decrypted application message. nullptr on decryption error.
   */
  std::unique_ptr<uint8_t[]> decryptMessage(const void *message);

private:
  const char *_key;
  const char *_secret;
};

#endif // __ESP_CRYPT_H__