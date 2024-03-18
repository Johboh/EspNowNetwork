#ifndef __DEVICE_FOOT_PEDAL_H__
#define __DEVICE_FOOT_PEDAL_H__

#include "Device.h"
#include "esp-now-device-structs.h"
#include <IMQTTRemote.h>
#include <functional>

/**
 * @brief A foot pedal. This represent a node that is running in a foot pedal, that upon tapping will send
 * either a tap or a long tap event.
 */
class DeviceFootPedal : public Device {
public:
  /**
   * @brief Construct a foot pedal.
   *
   * @param mqtt_remote the MQTT remote to use. Used to post data from the foot pedal.
   * @param mac_address the MAC address (as an uint64) for this device. Must be unique. You can find out the MAC
   * address by letting your device send a message and then either check the serial output of the router, or check the
   * <this-device-mqtt-client-id>/log/warning topic in MQTT.
   * @param name_suffix a human readable name suffix for the device. Used to indicate what foot pedal this is, in case
   * we have more than one.
   * @param on_click a callback that will be called when the foot pedal is tapped.
   */
  DeviceFootPedal(IMQTTRemote &mqtt_remote, uint64_t mac_address, std::string name_suffix,
                  std::function<void(uint8_t)> on_click);

public:
  std::string type() override { return "foot_pedal"; }
  uint64_t macAddress() override { return _mac_address; }
  std::string name() override { return "Foot pedal: " + _name_suffix; }
  bool onMessage(const uint8_t retries, const uint8_t version, const uint8_t *message) override;

private:
  void publish(EspNowFootPedalV1 *message, uint8_t retries);

private:
  uint64_t _mac_address;
  std::string _name_suffix;
  IMQTTRemote &_mqtt_remote;
  std::function<void(uint8_t)> _on_click;
};

#endif // __DEVICE_FOOT_PEDAL_H__