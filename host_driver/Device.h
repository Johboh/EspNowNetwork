#ifndef __DEVICE_H__
#define __DEVICE_H__

#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief Interface/Abstract class for a Device.
 * A node is a hardware that sends the ESP-NOW message, and a Device is the host represenation of this node.
 *
 * The Device will handle the incomming ESP-NOW message, forwared by the host.
 */
class Device {
public:
  /**
   * @brief A human readable name for this device, used for logging and debugging reasons. Should be unique in case
   * there are several instances of the same device. Example: "Bathroom motion"
   */
  virtual std::string name() = 0;

  /**
   * @brief The type of device, examples "motion", "light", "game-console".
   * This might be used as a path when checking for firmware or when logging to MQTT.
   * Must be URI safe, e.g. contains no "/" and characters safe to be in a URL and MQTT path.
   */
  virtual std::string type() = 0;

  /**
   * @brief The hardware for the device, in case we run same type of device but with different hardware or hardware
   * versions. This might be used as a path when checking for firmware. Must be URI safe, e.g. contains no "/" and
   * characters safe to be in a URL and MQTT path. Can be std::nullopt and thus this will not be used.
   */
  virtual std::optional<std::string> hardware() { return std::nullopt; }

  /**
   * @brief The MAC address for this device as an uint64.
   * Only messages from this device will be forwarded to the onMessage() function.
   */
  virtual uint64_t macAddress() = 0;

  /**
   * @brief Called periodially (from main loop typically). Can be used to to timing functions, resends etc.
   *
   * Convinience function for devices to potentially resend/reset after a certain period of time.
   *
   * @param last_message_received_at timestamp in milliseconds on when last (successful) message was received. 0 if no
   * message has been received.
   */
  virtual void handle(unsigned long last_message_received_at) {}

  /**
   * @brief Called by a device manager to indicate that there is a connection change. What "connection" mean is up to
   * the device manager, but usually this indicate that we are now connected to the Internet or to a MQTT server or some
   * other integration. This can be useful if the Device need to post som initial setup data to MQTT or similar.
   */
  virtual void onConnectionStateChanged(bool connected) {}

  /**
   * @brief Called on new message when the a message for this device has been received, e.g. when a message is received
   * that matched this device mac address, given by macAddress()
   *
   * @param retries number of retires it took to deliver this message to the host.
   * @param version message version.
   * @param message The message received. Cast this into your message(es) structure.
   * @return true if the message was accepted and handled.
   */
  virtual bool onMessage(const uint8_t retries, const uint8_t version, const uint8_t *message) = 0;
};

#endif // __DEVICE_H__