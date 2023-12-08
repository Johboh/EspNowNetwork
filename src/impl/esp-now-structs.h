#ifndef __ESP_NOW_STRUCTURE_H__
#define __ESP_NOW_STRUCTURE_H__

#include <cstdint>

#define MESSAGE_ID_HEADER 0x03

#define MESSAGE_ID_DISCOVERY_REQUEST_V1 0xD0
#define MESSAGE_ID_DISCOVERY_RESPONSE_V1 0xD1

#define MESSAGE_ID_CHALLENGE_REQUEST_V1 0xDA
#define MESSAGE_ID_CHALLENGE_RESPONSE_V1 0xDB
#define MESSAGE_ID_CHALLENGE_FIRMWARE_RESPONSE_V1 0xDC

#pragma pack(1)

/**
 * Sent by host and nodes. This message is followed by the application message.
 */
struct EspNowMessageHeaderV1 {
  uint8_t id = MESSAGE_ID_HEADER;
  uint16_t retries = 0;
  uint32_t header_challenge; // Challenge from [EspNowChallengeResponseV1]
};

/**
 * Sent by nodes to perform host discovery.
 */
struct EspNowDiscoveryRequestV1 {
  uint8_t id = MESSAGE_ID_DISCOVERY_REQUEST_V1;
  // The challenge that the host should send back/set in [EspNowDiscoveryResponseV1] reply.
  uint32_t discovery_challenge;
};

/**
 * Sent by host to confirm host discovery.
 */
struct EspNowDiscoveryResponseV1 {
  uint8_t id = MESSAGE_ID_DISCOVERY_RESPONSE_V1;
  uint32_t discovery_challenge; // Challenge from [EspNowDiscoveryRequestV1]
};

/**
 * Sent by nodes to request the challenge to include in the [EspNowMessageHeaderV1] message
 * for message verification (protect against reply attacks).
 */
struct EspNowChallengeRequestV1 {
  uint8_t id = MESSAGE_ID_CHALLENGE_REQUEST_V1;
  uint32_t firmware_version;
  // The challenge that the host should send back/set in [EspNowChallengeResponseV1] or
  // [EspNowChallengeFirmwareResponseV1] reply.
  uint32_t challenge_challenge;
};

/**
 * Sent by host in reply to a [EspNowChallengeRequestV1].
 * The challenge can only be used once.
 */
struct EspNowChallengeResponseV1 {
  uint8_t id = MESSAGE_ID_CHALLENGE_RESPONSE_V1;
  uint32_t challenge_challenge; // Challenge from [EspNowChallengeRequestV1].
  uint32_t header_challenge;    // Should be set in [EspNowMessageHeaderV1].
};

/**
 * Sent by host in reply to a [EspNowChallengeRequestV1] when the device should update its firmware.
 */
struct EspNowChallengeFirmwareResponseV1 {
  uint8_t id = MESSAGE_ID_CHALLENGE_FIRMWARE_RESPONSE_V1;
  uint32_t challenge_challenge; // Challenge from [EspNowChallengeRequestV1].
  char wifi_ssid[32];           // WiFi SSID that node should connect to.
  char wifi_password[64];       // WiFi password that the node should connect to.
  char url[96];                 // url where to find firmware binary. Note the max file path.
  char md5[32];                 // MD5 hash of firmware. Does not include trailing \0
};

#pragma pack(0)

#endif // __ESP_NOW_STRUCTURE_H__