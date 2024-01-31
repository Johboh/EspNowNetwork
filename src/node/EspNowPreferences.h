#ifndef __ESP_NOW_PREFERENCES_H__
#define __ESP_NOW_PREFERENCES_H__

#include "Preferences.h"
#include <nvs_flash.h>

/**
 * @brief Storage support on NVS flash.
 *
 */
class EspNowPreferences : public EspNowNetwork::Preferences {
public:
  /**
   * @brief Construct a new EspNowPreferences object
   */
  EspNowPreferences();

  /**
   * @brief Call before using to initialize NVS flash memory.
   * Might be omitted if already done elsewhere.
   */
  void initalizeNVS();

  /**
   * @brief Set the MAC address. Must be of size 6.
   */
  bool espNowSetMacForHost(uint8_t mac[6]) override;

  /**
   * @brief Return the MAC address stored, or std::nullopt if no mac stored.
   * @param buffer buffer to store MAC, must be of size 6 or larger.
   *
   * @param return true if MAC was read successfully.
   */
  bool espNowGetMacForHost(uint8_t *buffer) override;

  /**
   * @brief Set the WiFi channel that the host is on
   */
  bool espNowSetChannelForHost(uint8_t channel) override;

  /**
   * @brief Get the WiFi channel stored.  Returns false if no channel stored.
   * @param buffer buffer to store channel
   *
   * @param return true if channel was read successfully.
   */
  bool espNowGetChannelForHost(uint8_t *channel) override;

  /**
   * @brief After setting variables, call this to commit/save.
   * @return true on success.
   */
  bool commit() override;

  /**
   * @brief This will clear all data stored in NVS.
   * @return true on success.
   */
  bool eraseAll() override;

private:
  nvs_handle_t _nvs_handle;
};

#endif // __ESP_NOW_PREFERENCES_H__