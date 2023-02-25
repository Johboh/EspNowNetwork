#include "esp-now-structs.h"
#include <EspNowNode.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// Bits used for send ACKs to notify the _send_result_event_group Even Group.
#define SEND_SUCCESS_BIT 0x01
#define SEND_FAIL_BIT 0x02

// 500ms is a long time in the esp-now world. Used for both send and receive.
#define TICKS_TO_WAIT_FOR_EVENT (500 / portTICK_PERIOD_MS)

// Number of tries to resend the disovery message. Will wait for reply as long as defined by TICKS_TO_WAIT_FOR_EVENT
// between each message.
#define NUMBER_OF_RETRIES_FOR_DISCOVERY_REQUEST 10

// Number of times to try requesting a challenge. Will wait for reply as long as defined by TICKS_TO_WAIT_FOR_EVENT
// between each message.
#define NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST 10

// Keys for Preferences
#define PREF_KEY_HAVE_MAC "have-mac"
#define PREF_KEY_HOST_MAC_ADDRESS "host-mac-addr"

// Cannot be class members, as C callback esp_now_on_data_sent is not in class...
auto _send_result_event_group = xEventGroupCreate();

struct Element {
  size_t data_len = 0;
  uint8_t data[255]; // Max message size on ESP NOW is 250.
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
};

auto _receive_queue = xQueueCreate(5, sizeof(Element));

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

EspNowNode::EspNowNode(EspNowCrypt &crypt, OnLog on_log) : _crypt(crypt), _on_log(on_log) {}

bool EspNowNode::setup() {
  if (_setup_successful) {
    _on_log("Already have successful setup.", ESP_LOG_WARN);
    return true;
  }

  _preferences.begin("esp-now-node", false);

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);

  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_LORA_250K);

  // Init ESP-NOW
  esp_err_t r = esp_now_init();
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    _on_log("Error initializing ESP-NOW: " + String(errstr), ESP_LOG_ERROR);
    delay(5000);
    ESP.restart();
  } else {
    _on_log("Initializing ESP-NOW OK.", ESP_LOG_INFO);
  }

  r = esp_now_register_send_cb(esp_now_on_data_sent);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    _on_log("Registering send callback for esp now failed: " + String(errstr), ESP_LOG_ERROR);
  }

  r = esp_now_register_recv_cb(esp_now_on_data_callback);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    _on_log("Registering receive callback for esp now failed: " + String(errstr), ESP_LOG_ERROR);
  }

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
  bool presumably_valid_host_mac_address = _preferences.getBool(PREF_KEY_HAVE_MAC) &&
                                           _preferences.getBytesLength(PREF_KEY_HOST_MAC_ADDRESS) == ESP_NOW_ETH_ALEN;
  if (presumably_valid_host_mac_address) {
    _on_log("Presumably valid MAC address loaded.", ESP_LOG_INFO);
    _preferences.getBytes(PREF_KEY_HOST_MAC_ADDRESS, _esp_now_host_address, ESP_NOW_ETH_ALEN);
  } else {
    _on_log("No valid MAC address. Going into discovery mode.", ESP_LOG_INFO);
    memset(_esp_now_host_address, 0xFF, ESP_NOW_ETH_ALEN);
  }
  memcpy(peer_info.peer_addr, _esp_now_host_address, ESP_NOW_ETH_ALEN);

  r = esp_now_add_peer(&peer_info);
  bool success = r == ESP_OK;
  if (!success) {
    const char *errstr = esp_err_to_name(r);
    _on_log("Per adding failure: " + String(errstr), ESP_LOG_ERROR);
  }

  if (!presumably_valid_host_mac_address) {
    // Ok so we have no valid host MAC address.
    // Announce our precence until we get a reply.

    int8_t retries = NUMBER_OF_RETRIES_FOR_DISCOVERY_REQUEST;
    while (retries-- > 0) {
      // Send discovery message

      bool confirmed = false;
      uint8_t mac_addr[ESP_NOW_ETH_ALEN];
      EspNowDiscoveryRequestV1 message;
      _on_log("Sending broadcast discovery request", ESP_LOG_INFO);
      auto decrypted_data = sendAndWait((uint8_t *)&message, sizeof(EspNowDiscoveryRequestV1), mac_addr);
      if (decrypted_data != nullptr) {
        EspNowDiscoveryResponseV1 *message = (EspNowDiscoveryResponseV1 *)decrypted_data.get();
        confirmed = message->id == MESSAGE_ID_DISCOVERY_RESPONSE_V1;
      }

      if (confirmed) {
        _on_log("Got valid disovery response. Restarting.", ESP_LOG_INFO);
        _preferences.putBool(PREF_KEY_HAVE_MAC, true);
        _preferences.putBytes(PREF_KEY_HOST_MAC_ADDRESS, mac_addr, ESP_NOW_ETH_ALEN);
        _preferences.end();
        ESP.restart(); // Start over from the begining.
      }

      // No message/timeout or failed to verify. Try again.
    }
    _on_log("Failed to discover host. Setup failed.", ESP_LOG_ERROR);

    // So we never got a message after several retries.
    // Let caller now this.
    return false;
  }

  _setup_successful = success;
  return success;
}

bool EspNowNode::sendMessage(void *sub_message, size_t sub_message_size, int16_t retries) {
  if (!_setup_successful) {
    return false;
  }

  typedef EspNowMessageHeaderV1 Message;
  Message message;

  // First, we must request the challenge to use.
  bool got_challange = false;
  int8_t challenge_retries = NUMBER_OF_RETRIES_FOR_CHALLENGE_REQUEST;
  while (challenge_retries-- > 0) {
    EspNowChallengeRequestV1 request;
    _on_log("Sending challenge request.", ESP_LOG_INFO);
    auto decrypted_data = sendAndWait((uint8_t *)&request, sizeof(EspNowChallengeRequestV1));
    if (decrypted_data != nullptr) {
      EspNowChallengeResponseV1 *response = (EspNowChallengeResponseV1 *)decrypted_data.get();
      message.challenge = response->challenge;
      got_challange = true;
      _on_log("Got valid challenge response.", ESP_LOG_INFO);
      break;
    }
  }

  if (!got_challange) {
    _on_log("Failed to receive challenge response. Assuming invalid host MAC address. Clearing stored MAC address and "
            "restarting.",
            ESP_LOG_ERROR);
    // Sad times. We have no challenge. No point in continuing.
    // Assume host is broken.
    forgetHost();
    _preferences.end();
    ESP.restart();
    return false; // Unreachable, but just in case.
  }

  uint32_t size = sizeof(Message) + sub_message_size;
  std::unique_ptr<uint8_t[]> buff(new (std::nothrow) uint8_t[size]);
  memcpy(buff.get(), &message, sizeof(Message));
  memcpy(buff.get() + sizeof(Message), sub_message, sub_message_size);

  uint16_t attempt = 0;
  _on_log("Sending application message (" + String(attempt) + ")", ESP_LOG_INFO);
  sendMessageInternal(buff.get(), size);

  // If negative retries, don't wait.
  if (retries < 0) {
    return true;
  }

  bool success = false;
  while (attempt++ < retries) {
    auto bits = xEventGroupWaitBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT, pdTRUE, pdFALSE,
                                    TICKS_TO_WAIT_FOR_EVENT);
    if ((bits & SEND_SUCCESS_BIT) != 0) {
      _on_log("Message successfully delivered to host", ESP_LOG_DEBUG);
      success = true;
      break;
    } else {
      _on_log("Message failed to be delivered to host. Check host address. Will retry.", ESP_LOG_ERROR);
      // This is either a send fail bit set, or no bit set.
      // If no bit is set then its either a timeout from xEventGroupWaitBits or
      // something else. A timeout will almost never happen probably, as esp-now
      // is very fast in acking/nacking.
      delay(attempt * 5); // Backoff
      message.retries = attempt;
      memcpy(buff.get(), &message, sizeof(Message)); // "Refresh" message in buffer.
      _on_log("Sending application message (" + String(attempt) + ")", ESP_LOG_INFO);
      sendMessageInternal(buff.get(), size);
      continue;
    }
  }

  if (!success && attempt >= retries) {
    // Failed to get ACK on message. We have a valid host as we got challenge response above.
    _on_log("Failed to send message after retries.", ESP_LOG_ERROR);
    return false;
  }

  return success;
}

void EspNowNode::forgetHost() {
  _preferences.remove(PREF_KEY_HAVE_MAC);
  _preferences.remove(PREF_KEY_HOST_MAC_ADDRESS);
  memset(_esp_now_host_address, 0x00, ESP_NOW_ETH_ALEN);
  _setup_successful = false;
}

void EspNowNode::sendMessageInternal(uint8_t *buff, size_t length) {
  esp_err_t r = _crypt.sendMessage(_esp_now_host_address, buff, length);
  if (r != ESP_OK) {
    const char *errstr = esp_err_to_name(r);
    _on_log("_crypt.sendMessage() failure: " + String(errstr), ESP_LOG_ERROR);
  } else {
    _on_log("Message sent OK (not yet delivered)", ESP_LOG_DEBUG);
  }
}

std::unique_ptr<uint8_t[]> EspNowNode::sendAndWait(uint8_t *message, size_t length, uint8_t *out_mac_addr) {
  sendMessageInternal(message, length);

  // Wait for reply (with timeout)
  Element element;
  auto result = xQueueReceive(_receive_queue, &element, TICKS_TO_WAIT_FOR_EVENT);
  if (result == pdPASS) {
    if (out_mac_addr != nullptr) {
      memcpy(out_mac_addr, element.mac_addr, ESP_NOW_ETH_ALEN);
    }
    return _crypt.decryptMessage(element.data);
  }
  return nullptr;
}
