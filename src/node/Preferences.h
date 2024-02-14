#ifndef __ESP_NOW_I_PREFERENCES_H__
#define __ESP_NOW_I_PREFERENCES_H__

#include <cstdint>
#include <optional>

namespace EspNowNetwork {

#define MAC_ADDRESS_LENGTH 6

/**
 * @brief Non Volatile storage for persisting host MAC.
 *
 */
class Preferences {
public:
  /**
   * @brief Set the MAC address. Must be of size MAC_ADDRESS_LENGTH.
   */
  virtual bool espNowSetMacForHost(uint8_t mac[MAC_ADDRESS_LENGTH]) = 0;

  /**
   * @brief Return the MAC address stored, or std::nullopt if no mac stored.
   * @param buffer buffer to store MAC, must be of size MAC_ADDRESS_LENGTH or larger.
   *
   * @param return true if MAC was read successfully.
   */
  virtual bool espNowGetMacForHost(uint8_t *buffer) = 0;

  /**
   * @brief Set the WiFi channel that the host is on
   */
  virtual bool espNowSetChannelForHost(uint8_t channel) = 0;

  /**
   * @brief Get the WiFi channel stored.
   * Please note that the channel is not nessesarily a valid WiFi channel, it could be any uint8_t. Its validity must be
   * confirmed before used.
   *
   * @param return the channel stored, or std::nullopt if no channel.
   */
  virtual std::optional<uint8_t> espNowGetChannelForHost() = 0;

  /**
   * @brief Commit any changes written.
   * @return true on success.
   */
  virtual bool commit() = 0;

  /**
   * @brief Clear all variables (related to espNow).
   * @return true on success.
   */
  virtual bool eraseAll() = 0;
};

}; // namespace EspNowNetwork

#endif // __ESP_NOW_I_PREFERENCES_H__
