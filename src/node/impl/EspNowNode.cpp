#include "EspNowOta.h"
#include <EspNowNode.h>
#include <cstring>
#include <esp-now-structs.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

// Bits used for send ACKs to notify the _send_result_event_group Even Group.
#define SEND_SUCCESS_BIT 0x01
#define SEND_FAIL_BIT 0x02

#define TICKS_TO_WAIT_FOR_MESSAGE (100 / portTICK_PERIOD_MS) // 100ms
#define TICKS_TO_WAIT_FOR_ACK (100 / portTICK_PERIOD_MS)     // 100ms

// Number of tries to resend the disovery message. Will wait for reply as long as defined by TICKS_TO_WAIT_FOR_MESSAGE
// between each message.
#define NUMBER_OF_RETRIES_FOR_DISCOVERY_REQUEST 50

// Number of times to try requesting a challenge. Will wait for reply as long as defined by TICKS_TO_WAIT_FOR_MESSAGE
// between each message.
#define NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST 50

// We are using 2.4Ghz channels
#define WIFI_CHANNEL_LOWEST 1
#define WIFI_CHANNEL_HIGHEST 14 // 14 is technically possible to use, but it should be avoided and is very rarely used.

struct Element {
  size_t data_len = 0;
  uint8_t data[255]; // Max message size on ESP NOW is 250.
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
};

static QueueHandle_t _receive_queue = xQueueCreate(5, sizeof(Element));
static EventGroupHandle_t _send_result_event_group = xEventGroupCreate();

void EspNowNode::esp_now_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {

  // Set event bits based on result.
  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xEventGroupSetBitsFromISR(_send_result_event_group,
                                          status == ESP_NOW_SEND_SUCCESS ? SEND_SUCCESS_BIT : SEND_FAIL_BIT,
                                          &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void EspNowNode::esp_now_on_data_callback_legacy(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  // New message received on ESP-NOW.
  // Add to queue and leave callback as soon as we can.
  Element element;
  std::memcpy(element.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
  if (data_len > 0) {
    std::memcpy(element.data, data, std::min((size_t)data_len, sizeof(element.data)));
  }
  element.data_len = data_len;

  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xQueueSendFromISR(_receive_queue, &element, &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
void EspNowNode::esp_now_on_data_callback(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  esp_now_on_data_callback_legacy(esp_now_info->src_addr, data, data_len);
}
#endif

EspNowNode::EspNowNode(EspNowCrypt &crypt, EspNowNetwork::Preferences &preferences, uint32_t firmware_version,
                       OnStatus on_status, OnLog on_log, CrtBundleAttach crt_bundle_attach)
    : _on_log(on_log), _on_status(on_status), _crypt(crypt), _firmware_version(firmware_version),
      _crt_bundle_attach(crt_bundle_attach), _preferences(preferences) {

  _host_peer_info.ifidx = WIFI_IF_STA;
  _host_peer_info.channel = 0;     // Channel 0 means "use the same channel as WiFi". We don't use WiFi, but ESP-NOW is
                                   // using the MAC layer beneath.
  _host_peer_info.encrypt = false; // Never use esp NOW encryption. We run our own encryption (see EspNowCryp.h)
}

bool EspNowNode::setup() {
  if (_setup_successful) {
    log("Already have successful setup.", ESP_LOG_WARN);
    return true;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  esp_event_loop_create_default();
  _netif_sta = esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  // TODO(johboh): this might unset WIFI6 for ESP32-C6, but getting current protocols and appending WIFI_PROTOCOL_LR and
  // then setting them again, fails with bad argument. Presumably a bug in esp_wifi_set_protocol not supporting
  // WIFI_PROTOCOL_11AX?
  uint8_t protocol_bitmap = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR;
  ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, protocol_bitmap));

  // Init ESP-NOW
  esp_err_t r = esp_now_init();
  if (r != ESP_OK) {
    log("Error initializing ESP-NOW:", r);
    return false;
  } else {
    _esp_now_initialized = true;
    log("Initializing ESP-NOW OK.", ESP_LOG_INFO);
  }

  // Deprecated, but esp_now_set_peer_rate_config(peer_info.peer_addr, &esp_now_rate_config); does not work.
  // See https://github.com/espressif/esp-idf/issues/11751 and https://www.esp32.com/viewtopic.php?t=34546
  r = esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_250K);
  log("configuring espnow rate (legacy) failed:", r);

  r = esp_now_register_send_cb(esp_now_on_data_sent);
  log("Registering send callback for esp now failed:", r);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  r = esp_now_register_recv_cb(esp_now_on_data_callback);
#else
  r = esp_now_register_recv_cb(esp_now_on_data_callback_legacy);
#endif
  log("Registering receive callback for esp now failed:", r);

  // If we have host MAC address, add that one as a peer.
  // Else, add broadcast address and announce our presence.
  // If the mac we have stored is not valid it will be cleared and the setup will be unsuccessful.
  // Same logic apply for the WiFi channel. We try to get it, but on failure we will go into discovery mode.
  auto channel_opt = _preferences.espNowGetChannelForHost();
  bool presumably_valid_host_mac_address = _preferences.espNowGetMacForHost(_host_peer_info.peer_addr);
  bool presumably_valid_configuration = presumably_valid_host_mac_address && isValidWiFiChannel(channel_opt);

  if (presumably_valid_configuration) {
    auto channel = channel_opt.value();
    log("Presumably valid MAC address and WiFi channel (" + std::to_string(channel) + ") loaded.", ESP_LOG_INFO);
    auto r = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (r != ESP_OK) {
      // Failed to set channel, go into discovery mode. Could happen if this channel is not allowed in this country, see
      // https://en.wikipedia.org/wiki/List_of_WLAN_channels
      presumably_valid_configuration = false;
      log("Failed to set WiFi channel " + std::to_string(channel) + ":", r);
    }
  }

  if (!presumably_valid_configuration) {
    log("No valid MAC address and/or WiFi channel. Going into discovery mode.", ESP_LOG_INFO);
    std::memset(_host_peer_info.peer_addr, 0xFF, ESP_NOW_ETH_ALEN);
  }

  // Delete any existing peer. Fail silently (e.g. if not exists)
  esp_now_del_peer(_host_peer_info.peer_addr);

  r = esp_now_add_peer(&_host_peer_info);
  bool success = r == ESP_OK;
  log("Peer adding failure:", r);

  // If no valid configuration, we need to find the host MAC address as well as what WiFi channel we should use.
  if (!presumably_valid_configuration) {
    if (_on_status) {
      _on_status(Status::HOST_DISCOVERY_STARTED);
    }
    // Announce our precence until we get a reply.

    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    EspNowDiscoveryRequestV1 request;
    // The challenge we expect to get back in the disovery response.
    request.discovery_challenge = esp_random();

    int8_t retries = NUMBER_OF_RETRIES_FOR_DISCOVERY_REQUEST;
    uint8_t current_channel = WIFI_CHANNEL_LOWEST;

    // Discover host using a range of WiFi channels on the broadcast MAC address.
    while (retries-- > 0) {
      auto channel_to_test = current_channel++;
      if (current_channel > WIFI_CHANNEL_HIGHEST) {
        current_channel = WIFI_CHANNEL_LOWEST;
      }
      auto r = esp_wifi_set_channel(channel_to_test, WIFI_SECOND_CHAN_NONE);
      if (r != ESP_OK) {
        // Failed to set channel. Could happen if this channel is not allowed in this country,
        // see https://en.wikipedia.org/wiki/List_of_WLAN_channels
        log("Failed to set WiFi channel " + std::to_string(channel_to_test) +
                " in discovery mode, skipping this channel:",
            r);
        continue;
      }

      // Send discovery request
      log("Sending broadcast discovery request on channel " + std::to_string(channel_to_test) + " (" +
              std::to_string(NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST - retries - 1) + ")",
          ESP_LOG_INFO);
      auto decrypted_data = sendAndWait((uint8_t *)&request, sizeof(EspNowDiscoveryRequestV1), mac_addr);
      if (decrypted_data != nullptr) {
        EspNowDiscoveryResponseV1 *response = (EspNowDiscoveryResponseV1 *)decrypted_data.get();
        auto confirmed = response->id == MESSAGE_ID_DISCOVERY_RESPONSE_V1 &&
                         response->discovery_challenge == request.discovery_challenge &&
                         isValidWiFiChannel(response->channel);

        if (confirmed) {
          log("Got valid disovery response.", ESP_LOG_INFO);
          _preferences.espNowSetMacForHost(mac_addr);
          _preferences.espNowSetChannelForHost(response->channel);
          _preferences.commit();
          if (_on_status) {
            _on_status(Status::HOST_DISCOVERY_SUCCESSFUL);
          }
          // All good. Try to set wifi channel and set MAC and indicate we have a sucessful setup.
          auto r = esp_wifi_set_channel(response->channel, WIFI_SECOND_CHAN_NONE);
          if (r != ESP_OK) {
            // Failed to set channel. Could happen if this channel is not allowed in this country,
            // see https://en.wikipedia.org/wiki/List_of_WLAN_channels
            log("Failed to set WiFi channel " + std::to_string(response->channel) + " received from host:", r);
            break; // Unrecoverable. Give up.
          }

          // Ok all good. Copy MAC into host peer and add peer.
          std::memcpy(_host_peer_info.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
          r = esp_now_add_peer(&_host_peer_info);
          if (r != ESP_OK) {
            log("Failed to add peer:", r);
            break; // Unrecoverable. Give up.
          }

          _setup_successful = true;
          return true;
        } else {
          log("Got invalid disovery response. Retrying.", ESP_LOG_WARN);
        }
      }

      // No message/timeout or failed to verify. Try again.
    } // end of host discovery loop

    if (_on_status) {
      _on_status(Status::HOST_DISCOVERY_FAILED);
    }
    log("Failed to discover host. Setup failed.", ESP_LOG_ERROR);

    // So we never got a message after several retries.
    // Let caller now this.
    success = false;
  } // end of non valid configuration

  if (!success) {
    teardown(); // Teardown so we can try again.
  }

  _setup_successful = success;
  return success;
}

void EspNowNode::teardown() {
  _setup_successful = false;
  memset(_host_peer_info.peer_addr, 0x00, ESP_NOW_ETH_ALEN);

  esp_wifi_stop();

  if (_netif_sta != nullptr) {
    esp_netif_destroy_default_wifi(_netif_sta);
    _netif_sta = nullptr;
  }
  esp_event_loop_delete_default();
  esp_netif_deinit();

  // Can only call esp_now_deinit if we have initialized earlier.
  if (_esp_now_initialized) {
    esp_now_deinit();
    _esp_now_initialized = false;
  }

  esp_wifi_deinit();
}

bool EspNowNode::sendMessage(void *message, size_t message_size, int16_t retries) {
  if (!_setup_successful) {
    return false;
  }

  // Application message header
  EspNowMessageHeaderV1 header;

  EspNowChallengeRequestV1 request;
  // The challenge we expect to get back in the challenge/firmware response.
  request.challenge_challenge = esp_random();
  request.firmware_version = _firmware_version;

  // Hold any firmware update we might want to do. Null if no firmware update.
  std::unique_ptr<EspNowChallengeFirmwareResponseV1> firmware_update_response = nullptr;

  // First, we must request the challenge to use.
  bool got_challange = false;
  int8_t challenge_retries = NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST;
  while (!got_challange && challenge_retries-- > 0) {
    log("Sending challenge request (" +
            std::to_string(NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST - challenge_retries - 1) + ").",
        ESP_LOG_INFO);
    auto decrypted_data = sendAndWait((uint8_t *)&request, sizeof(EspNowChallengeRequestV1));
    if (decrypted_data != nullptr) {
      auto id = decrypted_data.get()[0];

      switch (id) {
      case MESSAGE_ID_CHALLENGE_RESPONSE_V1: {
        log("Got challenge response.", ESP_LOG_INFO);
        EspNowChallengeResponseV1 *response = (EspNowChallengeResponseV1 *)decrypted_data.get();
        // Validate the challenge for the challenge request/response pair
        if (response->challenge_challenge == request.challenge_challenge) {
          header.header_challenge = response->header_challenge;
          got_challange = true;
        } else {
          log("Challenge mismatch for challenge request/response (expected: " +
                  std::to_string(request.challenge_challenge) +
                  ", got: " + std::to_string(response->challenge_challenge) + ")",
              ESP_LOG_WARN);
        }
        break;
      }

      case MESSAGE_ID_CHALLENGE_FIRMWARE_RESPONSE_V1: {
        log("Got challenge update firmware response.", ESP_LOG_INFO);
        EspNowChallengeFirmwareResponseV1 *response = (EspNowChallengeFirmwareResponseV1 *)decrypted_data.get();
        // Validate the challenge for the challenge request/response pair
        if (response->challenge_challenge == request.challenge_challenge) {
          // Hosts wants us to update firmware. Lets do it. But first send our message.
          // We will update firmware after sending message.
          header.header_challenge = response->header_challenge;
          got_challange = true;
          // Hand over ownership of decrypted_data to firmware_update_response
          firmware_update_response = std::unique_ptr<EspNowChallengeFirmwareResponseV1>(
              reinterpret_cast<EspNowChallengeFirmwareResponseV1 *>(decrypted_data.release()));
        } else {
          log("Challenge mismatch for challenge request/ firmware response (expected: " +
                  std::to_string(request.challenge_challenge) +
                  ", got: " + std::to_string(response->challenge_challenge) + ")",
              ESP_LOG_WARN);
        }
        break;
      }

      } // end of switch(id).
    }
  } // end of discovery loop

  if (!got_challange) {
    log("Failed to receive challenge response. Assuming invalid host MAC address and/or WiFi channel. Clearing stored "
        "MAC address and WiFi channel. Node need to call setup() again to re-discover host.",
        ESP_LOG_ERROR);
    // Sad times. We have no challenge. No point in continuing.
    // Assume host is broken.
    forgetHost();
    if (_on_status) {
      _on_status(Status::INVALID_HOST);
    }
    teardown(); //  We need to setup again.
    return false;
  }

  uint32_t size = sizeof(EspNowMessageHeaderV1) + message_size;
  std::unique_ptr<uint8_t[]> buff(new (std::nothrow) uint8_t[size]);
  std::memcpy(buff.get(), &header, sizeof(EspNowMessageHeaderV1));
  std::memcpy(buff.get() + sizeof(EspNowMessageHeaderV1), message, message_size);

  uint16_t attempt = 0;
  log("Sending application message (" + std::to_string(attempt) + ")", ESP_LOG_INFO);
  xEventGroupClearBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT);
  sendMessageInternal(buff.get(), size);

  // If negative retries, don't wait.
  if (retries < 0) {
    return true;
  }

  bool success = false;
  while (attempt++ < retries) {
    auto bits = xEventGroupWaitBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT, pdTRUE, pdFALSE,
                                    TICKS_TO_WAIT_FOR_ACK);
    if ((bits & SEND_SUCCESS_BIT) != 0) {
      log("Message successfully delivered to host", ESP_LOG_DEBUG);
      success = true;
      break;
    } else {
      log("Message failed to be delivered to host. Check host address. Will retry.", ESP_LOG_ERROR);
      // This is either a send fail bit set, or no bit set.
      // If no bit is set then its either a timeout from xEventGroupWaitBits or
      // something else. A timeout will almost never happen probably, as esp-now
      // is very fast in acking/nacking.
      vTaskDelay(attempt * 5 / portTICK_PERIOD_MS); // Backoff
      header.retries = attempt;
      std::memcpy(buff.get(), &header, sizeof(EspNowMessageHeaderV1)); // "Refresh" message in buffer.
      log("Sending application message (" + std::to_string(attempt) + ")", ESP_LOG_INFO);
      xEventGroupClearBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT);
      sendMessageInternal(buff.get(), size);
      continue;
    }
  }

  // Regardless of outcome and we should update firmware, try to do that now.
  if (firmware_update_response != nullptr) {
    // Hosts wants us to update firmware. Lets do it.
    // handleFirmwareUpdate will never return.
    auto metadata = firmware_update_response.get();
    handleFirmwareUpdate(metadata->wifi_ssid, metadata->wifi_password, metadata->url, metadata->md5);
  }

  if (!success && attempt >= retries) {
    // Failed to get ACK on message. We have a valid host as we got challenge response above.
    log("Failed to send message after retries.", ESP_LOG_ERROR);
    return false;
  }

  return success;
}

void EspNowNode::forgetHost() {
  _preferences.eraseAll();
  _preferences.commit();
  memset(_host_peer_info.peer_addr, 0x00, ESP_NOW_ETH_ALEN);
}

void EspNowNode::sendMessageInternal(uint8_t *buff, size_t length) {
  esp_err_t r = _crypt.sendMessage(_host_peer_info.peer_addr, buff, length);
  if (r != ESP_OK) {
    log("_crypt.sendMessage() failure:", r);
  } else {
    log("Message sent OK (not yet delivered)", ESP_LOG_DEBUG);
  }
}

std::unique_ptr<uint8_t[]> EspNowNode::sendAndWait(uint8_t *message, size_t length, uint8_t *out_mac_addr) {
  xQueueReset(_receive_queue);
  sendMessageInternal(message, length);

  // Wait for reply (with timeout)
  Element element;
  auto result = xQueueReceive(_receive_queue, &element, TICKS_TO_WAIT_FOR_MESSAGE);
  if (result == pdPASS) {
    if (out_mac_addr != nullptr) {
      std::memcpy(out_mac_addr, element.mac_addr, ESP_NOW_ETH_ALEN);
    }
    return _crypt.decryptMessage(element.data);
  }
  return nullptr;
}

void EspNowNode::log(const std::string message, const esp_log_level_t log_level) {
  if (_on_log) {
    _on_log(message, log_level);
  }
}

void EspNowNode::log(const std::string message, const esp_err_t esp_err) {
  if (esp_err != ESP_OK) {
    const char *errstr = esp_err_to_name(esp_err);
    log(message + " " + std::string(errstr), ESP_LOG_ERROR);
  }
}

void EspNowNode::handleFirmwareUpdate(char *wifi_ssid, char *wifi_password, char *url, char *md5) {
  if (_on_status) {
    _on_status(Status::FIRMWARE_UPDATE_STARTED);
  }

  // Stop ESP-NOW and any other wifi related things before trying to update firmware.
  teardown();

  // Connect to wifi.
  EspNowOta _esp_now_ota(
      [&](const std::string message, const esp_log_level_t log_level) { log("EspNowOta: " + message, log_level); },
      _crt_bundle_attach);

  uint16_t retries = 2;
  unsigned long connect_timeout_ms = 15000;
  if (!_esp_now_ota.connectToWiFi(wifi_ssid, wifi_password, connect_timeout_ms, retries)) {
    log("Connection to WiFi failed! Restarting...", ESP_LOG_ERROR);
    if (_on_status) {
      _on_status(Status::FIRMWARE_UPDATE_WIFI_SETUP_FAILED);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
  }

  // Ok we have WiFi.
  // Download file.
  auto urlstr = std::string(url);
  auto md5str = std::string(md5, md5 + 32);
  bool success = _esp_now_ota.updateFrom(urlstr, md5str);

  if (success) {
    log("Firwmare update successful. Rebooting.", ESP_LOG_INFO);
    if (_on_status) {
      _on_status(Status::FIRMWARE_UPDATE_SUCCESSFUL);
    }
  } else {
    log("Firwmare update failed. Rebooting.", ESP_LOG_ERROR);
    if (_on_status) {
      _on_status(Status::FIRMWARE_UPDATE_FAILED);
    }
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  esp_restart();
}

bool EspNowNode::isValidWiFiChannel(uint8_t channel) {
  return channel >= WIFI_CHANNEL_LOWEST && channel <= WIFI_CHANNEL_HIGHEST;
}

bool EspNowNode::isValidWiFiChannel(std::optional<uint8_t> &channel_opt) {
  if (channel_opt) {
    auto channel = channel_opt.value();
    return isValidWiFiChannel(channel);
  } else {
    return false;
  }
}