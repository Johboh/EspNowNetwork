#include <EspNowNode.h>
#include "EspNowOta.h"
#include "esp-now-structs.h"
#include <cstring>
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

// wifi channels that will be tried during discovery
#define WIFI_SCAN_CHANNELS {1,2,3,4,5,6,7,8,9,10,11,12,13,14}

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
      _crt_bundle_attach(crt_bundle_attach), _preferences(preferences) {}

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
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
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
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
  } else {
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

  esp_now_peer_info_t peer_info;
  peer_info.ifidx = WIFI_IF_STA;
  // Channel 0 means "use the current channel which station or softap is on". We should hardcode this to a specific
  // channel so we for sure use same channel on both router and nodes.
  peer_info.channel = 0;
  peer_info.encrypt = false; // Never use esp NOW encryption.

  // If we have host MAC address, add that one as a peer.
  // Else, add broadcast address and announce our presence.
  // If the mac we have stored is not valid, we will fail when sending messages,
  // and will clear the MAC we have and restart, and thus end up here again.
  bool presumably_valid_host_mac_address = _preferences.espNowGetMacForHost(_esp_now_host_address);
  if (presumably_valid_host_mac_address) {
    log("Presumably valid MAC address loaded.", ESP_LOG_INFO);
    uint8_t channel = 1;
    _preferences.espNowGetChannelForHost(&channel);
    log("loaded channel " + std::to_string(channel), ESP_LOG_INFO);
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
  } else {
    log("No valid MAC address. Going into discovery mode.", ESP_LOG_INFO);
    std::memset(_esp_now_host_address, 0xFF, ESP_NOW_ETH_ALEN);
  }
  std::memcpy(peer_info.peer_addr, _esp_now_host_address, ESP_NOW_ETH_ALEN);

  // Delete any existing peer. Fail silently (e.g. if not exists)
  esp_now_del_peer(peer_info.peer_addr);

  r = esp_now_add_peer(&peer_info);
  bool success = r == ESP_OK;
  if (!success) {
    log("Per adding failure:", r);
  }

  if (!presumably_valid_host_mac_address) {
    if (_on_status) {
      _on_status(Status::HOST_DISCOVERY_STARTED);
    }
    // Ok so we have no valid host MAC address.
    // Announce our precence until we get a reply.

    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    EspNowDiscoveryRequestV1 request;
    // The challenge we expect to get back in the disovery response.
    request.discovery_challenge = esp_random();

    int8_t retries = NUMBER_OF_RETRIES_FOR_DISCOVERY_REQUEST;

    uint8_t channels[] = WIFI_SCAN_CHANNELS;
    uint8_t channelCount = sizeof(channels);
    uint8_t channelIdx = 0;

    while (retries-- > 0) {
      uint8_t channel = channels[channelIdx++ % channelCount];
      log("setting discovery channel " + std::to_string(channel), ESP_LOG_INFO);
      ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
      ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
      ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

      // Send discovery request
      bool confirmed = false;
      log("Sending broadcast discovery request (" +
              std::to_string(NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST - retries - 1) + ")",
          ESP_LOG_INFO);
      auto decrypted_data = sendAndWait((uint8_t *)&request, sizeof(EspNowDiscoveryRequestV1), mac_addr);
      if (decrypted_data != nullptr) {
        EspNowDiscoveryResponseV1 *response = (EspNowDiscoveryResponseV1 *)decrypted_data.get();
        confirmed = response->id == MESSAGE_ID_DISCOVERY_RESPONSE_V1 &&
                    response->discovery_challenge == request.discovery_challenge;
        if (confirmed) {
          channel = response->channel;
          log("received channel " + std::to_string(channel) + " from discovery response", ESP_LOG_INFO);
        }
      }

      if (confirmed) {
        log("Got valid disovery response. Restarting.", ESP_LOG_INFO);
        _preferences.espNowSetMacForHost(mac_addr);
        _preferences.espNowSetChannelForHost(channel);        
        _preferences.commit();
        if (_on_status) {
          _on_status(Status::HOST_DISCOVERY_SUCCESSFUL);
        }
        esp_restart(); // Start over from the begining.
      }

      // No message/timeout or failed to verify. Try again.
    }
    if (_on_status) {
      _on_status(Status::HOST_DISCOVERY_FAILED);
    }
    log("Failed to discover host. Setup failed.", ESP_LOG_ERROR);

    // So we never got a message after several retries.
    // Let caller now this.
    return false;
  }

  _setup_successful = success;
  return success;
}

void EspNowNode::teardown() {
  _setup_successful = false;
  memset(_esp_now_host_address, 0x00, ESP_NOW_ETH_ALEN);

  esp_wifi_stop();
  if (_netif_sta != nullptr) {
    esp_netif_destroy_default_wifi(_netif_sta);
  }
  esp_event_loop_delete_default();
  esp_netif_deinit();
  esp_now_deinit();
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
          // Hosts wants us to update firmware. Lets do it.
          // handleFirmwareUpdate will never return.
          handleFirmwareUpdate(response->wifi_ssid, response->wifi_password, response->url, response->md5);
        } else {
          log("Challenge mismatch for challenge request/ firmware response (expected: " +
                  std::to_string(request.challenge_challenge) +
                  ", got: " + std::to_string(response->challenge_challenge) + ")",
              ESP_LOG_WARN);
        }
        break;
      }
      }
    }
  }

  if (!got_challange) {
    log("Failed to receive challenge response. Assuming invalid host MAC address. Clearing stored MAC address and "
        "restarting.",
        ESP_LOG_ERROR);
    // Sad times. We have no challenge. No point in continuing.
    // Assume host is broken.
    forgetHost();
    if (_on_status) {
      _on_status(Status::INVALID_HOST);
    }
    esp_restart();
    return false; // Unreachable, but just in case.
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
  memset(_esp_now_host_address, 0x00, ESP_NOW_ETH_ALEN);
  _setup_successful = false;
}

void EspNowNode::sendMessageInternal(uint8_t *buff, size_t length) {
  esp_err_t r = _crypt.sendMessage(_esp_now_host_address, buff, length);
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
