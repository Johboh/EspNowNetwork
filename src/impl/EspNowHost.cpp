#include <EspNowHost.h>

#include "esp-now-structs.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Bits used for send ACKs to notify the _send_result_event_group Even Group.
#define SEND_SUCCESS_BIT 0x01
#define SEND_FAIL_BIT 0x02

struct Element {
  size_t data_len = 0;
  uint8_t data[255]; // Max message size on ESP-NOW is 250.
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
};

auto _send_result_event_group = xEventGroupCreate();
auto _receive_queue = xQueueCreate(10, sizeof(Element));

void esp_now_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Set event bits based on result.
  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xEventGroupSetBitsFromISR(_send_result_event_group,
                                          status == ESP_NOW_SEND_SUCCESS ? SEND_SUCCESS_BIT : SEND_FAIL_BIT,
                                          &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void esp_now_on_data_callback(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  // New message received on ESP-NOW.
  // Add to queue and leave callback as soon as we can.
  Element element;
  memcpy(element.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
  if (data_len > 0) {
    memcpy(element.data, data, min((size_t)data_len, sizeof(element.data)));
  }
  element.data_len = data_len;

  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xQueueSendFromISR(_receive_queue, &element, &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

EspNowHost::EspNowHost(EspNowCrypt &crypt, OnNewMessage on_new_message, OnApplicationMessage on_application_message,
                       FirmwareUpdateAvailable firwmare_update, OnLog on_log)
    : _crypt(crypt), _on_log(on_log), _on_new_message(on_new_message), _firwmare_update(firwmare_update),
      _on_application_message(on_application_message) {}

bool EspNowHost::setup() {
  esp_err_t r = esp_now_init();
  if (r != 0) {
    const char *errstr = esp_err_to_name(r);
    log("Error initializing ESP-NOW: " + String(errstr), ESP_LOG_ERROR);
    delay(5000);
    ESP.restart();
  } else {
    log("Initializing ESP-NOW OK.", ESP_LOG_INFO);
  }

  esp_wifi_config_espnow_rate(WIFI_IF_AP, WIFI_PHY_RATE_LORA_250K);

  r = esp_now_register_recv_cb(esp_now_on_data_callback);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    log("Registering receive callback for ESP-NOW failed: " + String(errstr), ESP_LOG_ERROR);
  }

  r = esp_now_register_send_cb(esp_now_on_data_sent);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    log("Registering send callback for esp now failed: " + String(errstr), ESP_LOG_ERROR);
  }

  return r == ESP_OK;
}

void EspNowHost::handle() {
  Element element;
  auto result = xQueueReceive(_receive_queue, &element, 1); // Only wait one tick.
  if (result == pdPASS) {
    // We have a new message!
    ++_number_of_messages;
    if (_on_new_message) {
      _on_new_message(); // Notify.
    }

    auto decrypted_data = _crypt.decryptMessage(element.data);
    if (decrypted_data != nullptr) {
      handleQueuedMessage(element.mac_addr, decrypted_data.get());
    } else {
      uint64_t mac_address = macToMac(element.mac_addr);
      log("Failed to decrypt message received from 0x" + String(mac_address, HEX), ESP_LOG_WARN);
    }
  }

  auto bits = xEventGroupWaitBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT, pdTRUE, pdFALSE, 1);
  if ((bits & SEND_SUCCESS_BIT) != 0) {
    log("Message delivered.", ESP_LOG_INFO);
  }
  if ((bits & SEND_FAIL_BIT) != 0) {
    log("Message fail to deliver.", ESP_LOG_INFO);
  }
}

void EspNowHost::handleQueuedMessage(uint8_t *mac_addr, uint8_t *data) {
  uint64_t mac_address = macToMac(mac_addr);

  MessageMetadata metadata;
  metadata.mac_address = mac_address;

  uint8_t id = data[0];
  switch (id) {
  case MESSAGE_ID_HEADER: {
    typedef EspNowMessageHeaderV1 Message;
    auto *message = (Message *)data;
    log("Got application message from 0x" + String(mac_address, HEX) +
            " with challange: " + String(message->header_challenge),
        ESP_LOG_INFO);
    // Verify challenge.
    auto challenge = _challenges.find(mac_address);
    if (challenge != _challenges.end()) {
      auto expected_challenge = challenge->second;
      if (expected_challenge == message->header_challenge) {
        metadata.retries = message->retries;
        auto outer_message_size = sizeof(Message);
        const uint8_t *inner_message = data + outer_message_size;
        if (_on_application_message) {
          _on_application_message(metadata, inner_message);
        }
      } else {
        log("Challenge mismatch (expected: " + String(expected_challenge) +
                ", got: " + String(message->header_challenge) + ") for 0x" + String(mac_address, HEX),
            ESP_LOG_WARN);
      }
      // Remove previous challenge (even on mismatch to prevent brute force)
      _challenges.erase(mac_address);
    } else {
      log("No challenge registered for 0x" + String(mac_address, HEX) +
              " (challenge received: " + String(message->header_challenge) + ")",
          ESP_LOG_WARN);
    }

    break;
  }
  case MESSAGE_ID_DISCOVERY_REQUEST_V1: {
    EspNowDiscoveryRequestV1 *message = (EspNowDiscoveryRequestV1 *)data;
    log("Got discovery request from 0x" + String(mac_address, HEX) + " and sending reply.", ESP_LOG_INFO);
    handleDiscoveryRequest(mac_addr, message->discovery_challenge);
    break;
  }
  case MESSAGE_ID_CHALLENGE_REQUEST_V1: {
    EspNowChallengeRequestV1 *message = (EspNowChallengeRequestV1 *)data;
    auto firmware_version = message->firmware_version;
    log("Got challenge request from 0x" + String(mac_address, HEX) + ", firmware version: " + String(firmware_version),
        ESP_LOG_INFO);
    handleChallengeRequest(mac_addr, message->challenge_challenge, firmware_version);
    break;
  }

  default:
    log("Received message with unknown id from device with MAC address 0x" + String(mac_address, HEX) + ". Got id: 0x" +
            String(id, HEX),
        ESP_LOG_WARN);
    break;
  }
}

void EspNowHost::handleDiscoveryRequest(uint8_t *mac_addr, uint32_t discovery_challenge) {
  EspNowDiscoveryResponseV1 message;
  message.discovery_challenge = discovery_challenge;
  sendMessageToTemporaryPeer(mac_addr, &message, sizeof(EspNowDiscoveryResponseV1));
}

void EspNowHost::handleChallengeRequest(uint8_t *mac_addr, uint32_t challenge_challenge, uint32_t firmware_version) {
  uint64_t mac_address = macToMac(mac_addr);

  // Any firmware to update?
  if (_firwmare_update) {
    auto metadata = _firwmare_update(mac_address, firmware_version);
    if (metadata) {
      log("Sending firmware update response to 0x" + String(mac_address, HEX), ESP_LOG_INFO);
      EspNowChallengeFirmwareResponseV1 message;
      message.challenge_challenge = challenge_challenge;
      strncpy(message.wifi_ssid, metadata->wifi_ssid, sizeof(message.wifi_ssid));
      strncpy(message.wifi_password, metadata->wifi_password, sizeof(message.wifi_password));
      strncpy(message.url, metadata->url, sizeof(message.url));
      sendMessageToTemporaryPeer(mac_addr, &message, sizeof(EspNowChallengeFirmwareResponseV1));
      return;
    }
  }

  // No firmware update (early return above)
  EspNowChallengeResponseV1 message;
  message.challenge_challenge = challenge_challenge;
  // Not sure how we want to do it here. For now, if we already have a challenge, don't generate a new one.
  // We always remove a challenge once it has been used, or o challenge verification failure.
  // We re-use any not yet challanged challange in so the node get same challange back in case
  // they send several challange requests in a row (i.e. miss the first reply).
  // This is to provent any potential out of sync issues.
  auto challenge = _challenges.find(mac_address);
  if (challenge != _challenges.end()) {
    // Existing one, reuse.
    message.header_challenge = challenge->second;
  } else {
    // No existing one, create new one.
    message.header_challenge = esp_random();
    _challenges[mac_address] = message.header_challenge;
  }
  log("Sending challenge response to 0x" + String(mac_address, HEX) + " with challenge " +
          String(message.header_challenge),
      ESP_LOG_INFO);
  sendMessageToTemporaryPeer(mac_addr, &message, sizeof(EspNowChallengeResponseV1));
}

void EspNowHost::sendMessageToTemporaryPeer(uint8_t *mac_addr, void *message, size_t length) {
  esp_now_peer_info_t peer_info;
  peer_info.ifidx = WIFI_IF_AP;
  // Channel 0 means "use the current channel which station or softap is on". We should hardcode this to a specific
  // channel so we for sure use same channel on both router and nodes.
  peer_info.channel = 0;
  peer_info.encrypt = false; // Never use esp NOW encryption.
  memcpy(peer_info.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

  esp_err_t r = esp_now_add_peer(&peer_info);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    log("esp_now_add_peer failure: " + String(errstr), ESP_LOG_ERROR);
  }

  r = _crypt.sendMessage(mac_addr, message, length);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    log("_crypt.sendMessage() failure: " + String(errstr), ESP_LOG_ERROR);
  } else {
    log("Message sent OK (not yet delivered)", ESP_LOG_INFO);
  }

  // We are done with the peer.
  r = esp_now_del_peer(mac_addr);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    log("esp_now_del_peer failure: " + String(errstr), ESP_LOG_ERROR);
  }
}

uint64_t EspNowHost::macToMac(uint8_t *mac_addr) {
  return ((uint64_t)mac_addr[0] << 40) + ((uint64_t)mac_addr[1] << 32) + ((uint64_t)mac_addr[2] << 24) +
         ((uint64_t)mac_addr[3] << 16) + ((uint64_t)mac_addr[4] << 8) + ((uint64_t)mac_addr[5]);
}

void EspNowHost::log(const String message, const esp_log_level_t log_level) {
  if (_on_log) {
    String messag_with_counter = "[#" + String(_number_of_messages) + "] " + message;
    _on_log(messag_with_counter, log_level);
  }
}