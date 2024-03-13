#ifndef __DEVICE_UTILS_H__
#define __DEVICE_UTILS_H__

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sstream>
#include <string>

namespace DeviceUtils {
static std::string toHex(uint64_t i) {
  std::stringstream sstream;
  sstream << std::hex << i;
  return sstream.str();
}

// ESP-NOW compatible millis()
static inline unsigned long millis() { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

} // namespace DeviceUtils

#endif // __DEVICE_UTILS_H__