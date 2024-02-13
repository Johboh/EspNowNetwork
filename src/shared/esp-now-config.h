#ifndef __ESP_NOW_CONFIG_H__
#define __ESP_NOW_CONFIG_H__

#include <cstdint>

/**
 * @brief Basic configuration metadata.
 */
struct ConfigurationHeader {
  uint64_t revision;
  uint8_t length;
};

#endif // __ESP_NOW_CONFIG_H__
