#ifndef __ESP_NOW_CONFIG_H__
#define __ESP_NOW_CONFIG_H__

#include <cstdint>

/**
 * A container for the application-specific config data that is held within the payload.
 * The payload could be anything, like a string, or json, but is probably a struct for efficiency.
 */
struct EspNowConfigEnvelope {
  uint16_t version; // an ID that should be unique for each unique config/payload
  uint8_t length;   // length of the payload
  uint8_t payload[150];
};

#endif // __ESP_NOW_CONFIG_H__