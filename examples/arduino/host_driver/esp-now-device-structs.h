#ifndef __ESP_NOW_DEVICE_STRUCTURES_H__
#define __ESP_NOW_DEVICE_STRUCTURES_H__

#include <cstdint>

#pragma pack(1)

#define CLICK_NONE 0
#define CLICK_SINGLE 1
#define CLICK_DOUBLE 2
#define CLICK_LONG 3

struct EspNowFootPedalV1 {
  uint8_t version = 0x01;
  uint8_t click = 0;
  double battery_voltage = -1;
  double temperature = -1;
};

#pragma pack(0)

#endif // __ESP_NOW_DEVICE_STRUCTURES_H__