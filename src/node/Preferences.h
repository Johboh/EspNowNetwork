#ifndef __ESP_NOW_I_PREFERENCES_H__
#define __ESP_NOW_I_PREFERENCES_H__

#include <cstdint>
#include "esp-now-config.h"

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

  virtual bool getConfig(EspNowConfigEnvelope *config_envelope) = 0;
  virtual bool setConfig(EspNowConfigEnvelope *config_envelope) = 0;

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