#ifndef __ESP_NOW_PREFERENCES_H__
#define __ESP_NOW_PREFERENCES_H__

#include <nvs_flash.h>
#include <string>

/**
 * @brief Storage support on NVS flash.
 *
 */
class EspNowPreferences {
public:
  /**
   * @brief Construct a new EspNowPreferences object
   */
  EspNowPreferences();

  void init();

  // Specific settings stored in NVS
  bool hasMac();
  void setHasMac(bool blink);

  bool setMac(uint8_t *mac, size_t length);
  size_t getMacLength();
  bool getMac(uint8_t *buffer, size_t length);

  /**
   * @brief After setting variables, call this to commit/save.
   * @return true on success.
   */
  bool commit();

  /**
   * @brief This will clear all data stored in NVS.
   * @return true on success.
   */
  bool eraseAll();

private:
  uint8_t getU8(const std::string &key);
  void setU8(const std::string &key, const uint8_t value, bool commit = false);
  bool getBool(const std::string &key) { return getU8(key) > 0; }
  void setBool(const std::string &key, const bool value, bool commit = false) { setU8(key, value ? 1 : 0, commit); }

  nvs_handle_t _nvs_handle;
};

#endif // __ESP_NOW_PREFERENCES_H__