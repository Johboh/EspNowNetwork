#include <EspNowHost.h>
#include "esp-now-structs.h"
#include <cstring>
#include <esp_random.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <sstream>
#include <RingBuf.h>

// Bits used for send ACKs to notify the _send_result_event_group Even Group.
#define SEND_SUCCESS_BIT 0x01
#define SEND_FAIL_BIT 0x02

#define WIFI_PKT_VSC_CATCODE 127
#define WIFI_PKT_VSC_ELEMENT 221
#define WIFI_PKT_ESPNOW_TYPE 4
#define ESPRESSIF_MAC_PREFIX {0x18, 0xFE, 0x34} // 0x18FE34 is the first 3 bytes of Espressif MAC addresses

struct Element {
  size_t data_len = 0;
  uint8_t data[255]; // Max message size on ESP-NOW is 250.
  uint8_t mac_addr[ESP_NOW_ETH_ALEN];
};

static QueueHandle_t _receive_queue = xQueueCreate(10, sizeof(Element));
static EventGroupHandle_t _send_result_event_group = xEventGroupCreate();

typedef struct {
  uint8_t mac[6];
} __attribute__((packed)) MacAddr;

// ESPNow Frame Format
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
typedef struct {
  int16_t fctl;
  int16_t duration;
  MacAddr da;
  MacAddr sa;
  MacAddr bssid;
  int16_t seqctl;
  uint8_t catcode;
  uint8_t orgid[3];
  uint8_t random[4];
  unsigned char payload[];
} __attribute__((packed)) WifiMgmtHdr;

// vendor specific content section
typedef struct {
  uint8_t elId;
  uint8_t len;
  uint8_t orgid[3];
  uint8_t type;
  uint8_t ver;
  unsigned char body[];
} __attribute__((packed)) EspNowVsc;

const wifi_promiscuous_filter_t filt={
    .filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT
};

typedef struct {
  uint8_t mac[6];
  int rssi;
} __attribute__((packed)) MacToRssi;

// a queue to hold recent mac_addr/rssi records
// a workaround until this feature request is implemented: https://github.com/espressif/arduino-esp32/issues/7992
RingBuf<MacToRssi, 16> macBuffer;

void EspNowHost::esp_wifi_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  wifi_pkt_rx_ctrl_t ctrl = (wifi_pkt_rx_ctrl_t)pkt->rx_ctrl;
  int len = pkt->rx_ctrl.sig_len;
  WifiMgmtHdr *wh = (WifiMgmtHdr*)pkt->payload;
  len -= sizeof(WifiMgmtHdr);
  if (len < 0){
    return;
  }

  // check that the packet is ESP-NOW
  uint8_t espressifId[] = ESPRESSIF_MAC_PREFIX;
  if (wh->catcode != WIFI_PKT_VSC_CATCODE || 
      memcmp(wh->orgid, espressifId, sizeof(espressifId)) != 0) {
    return;
  }

  EspNowVsc *vsc = (EspNowVsc*)wh->payload;
  if (vsc->elId != WIFI_PKT_VSC_ELEMENT || 
      vsc->type != WIFI_PKT_ESPNOW_TYPE || 
      memcmp(vsc->orgid, espressifId, sizeof(espressifId)) != 0) {
    return;
  }

  MacToRssi macToRssi;
  macToRssi.rssi = ctrl.rssi;
  std::memcpy(macToRssi.mac, wh->sa.mac, sizeof(macToRssi.mac));

  macBuffer.pushOverwrite(macToRssi);
  //ESP_LOGI("foo", "promisc mac=" MACSTR " catcode=%d rssi=%d", MAC2STR(wh->sa.mac), wh->catcode, ctrl.rssi);
}

int EspNowHost::getRssi(const uint8_t *mac_addr) {
  MacToRssi macToRssi;
  uint16_t idx = macBuffer.size();
  while (--idx > 0) {
    macToRssi = macBuffer[idx];
    if (memcmp(macToRssi.mac, mac_addr, sizeof(macToRssi.mac)) == 0) {
      return macToRssi.rssi;
    }
  }
  return 0;
}

void EspNowHost::esp_now_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Set event bits based on result.
  auto xHigherPriorityTaskWoken = pdFALSE;
  auto result = xEventGroupSetBitsFromISR(_send_result_event_group,
                                          status == ESP_NOW_SEND_SUCCESS ? SEND_SUCCESS_BIT : SEND_FAIL_BIT,
                                          &xHigherPriorityTaskWoken);
  if (result != pdFAIL && xHigherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

void EspNowHost::esp_now_on_data_callback_legacy(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
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
void EspNowHost::esp_now_on_data_callback(const esp_now_recv_info_t *esp_now_info, const uint8_t *data, int data_len) {
  esp_now_on_data_callback_legacy(esp_now_info->src_addr, data, data_len);
}
#endif

EspNowHost::EspNowHost(EspNowCrypt &crypt, EspNowHost::WiFiInterface wifi_interface, OnNewMessage on_new_message,
                       OnApplicationMessage on_application_message, FirmwareUpdateAvailable firwmare_update,
                       OnLog on_log)
    : _crypt(crypt), _wifi_interface(wifi_interface), _on_log(on_log), _on_new_message(on_new_message),
      _firwmare_update(firwmare_update), _on_application_message(on_application_message) {}

void EspNowHost::newMessageTask(void *pvParameters) {
  EspNowHost *_this = (EspNowHost *)pvParameters;

  while (1) {
    Element element;
    auto result = xQueueReceive(_receive_queue, &element, portMAX_DELAY);
    if (result == pdPASS) {
      // We have a new message!
      if (_this->_on_new_message) {
        _this->_on_new_message(); // Notify.
      }

      auto decrypted_data = _this->_crypt.decryptMessage(element.data);
      if (decrypted_data != nullptr) {
        _this->handleQueuedMessage(element.mac_addr, decrypted_data.get());
      } else {
        uint64_t mac_address = _this->macToMac(element.mac_addr);
        _this->log("Failed to decrypt message received from 0x" + _this->toHex(mac_address), ESP_LOG_WARN);
      }
    }
  }
}
void EspNowHost::messageDeliveredTask(void *pvParameters) {
  EspNowHost *_this = (EspNowHost *)pvParameters;

  while (1) {
    auto bits =
        xEventGroupWaitBits(_send_result_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if ((bits & SEND_SUCCESS_BIT) != 0) {
      _this->log("Message delivered.", ESP_LOG_INFO);
    }
    if ((bits & SEND_FAIL_BIT) != 0) {
      _this->log("Message fail to deliver.", ESP_LOG_INFO);
    }
  }
}

bool EspNowHost::start() {
#if CONFIG_IDF_TARGET_ESP32C6 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  uint8_t protocol_bitmap =
      WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX | WIFI_PROTOCOL_LR;
#else
  uint8_t protocol_bitmap = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR;
#endif
  ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, protocol_bitmap));

  esp_err_t r = esp_now_init();
  if (r != 0) {
    log("Error initializing ESP-NOW:", r);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
  } else {
    log("Initializing ESP-NOW OK.", ESP_LOG_INFO);
  }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  r = esp_now_register_recv_cb(esp_now_on_data_callback);
#else
  r = esp_now_register_recv_cb(esp_now_on_data_callback_legacy);
#endif
  log("Registering receive callback for ESP-NOW failed:", r);

  r = esp_now_register_send_cb(esp_now_on_data_sent);
  log("Registering send callback for esp now failed:", r);

  r = esp_wifi_set_promiscuous_filter(&filt);
  log("Registering promisc filter failed:", r);

  r = esp_wifi_set_promiscuous_rx_cb(&esp_wifi_promiscuous_rx_cb);
  log("Registering callback for promisc failed:", r);

  esp_wifi_set_promiscuous(true);

  auto ok = r == ESP_OK;

  if (ok) {
    xTaskCreate(newMessageTask, "new_message_task", 4096, this, 5, NULL);
    xTaskCreate(messageDeliveredTask, "message_delivered_task", 4096, this, 10, NULL);
  }

  return ok;
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
    log("Got application message from 0x" + toHex(mac_address) +
            " with challange: " + std::to_string(message->header_challenge),
        ESP_LOG_INFO);
    // Verify challenge.
    auto challenge = _challenges.find(mac_address);
    if (challenge != _challenges.end()) {
      auto expected_challenge = challenge->second;
      if (expected_challenge == message->header_challenge) {
        metadata.retries = message->retries;
        metadata.rssi = getRssi(mac_addr);
        auto outer_message_size = sizeof(Message);
        const uint8_t *inner_message = data + outer_message_size;
        if (_on_application_message) {
          _on_application_message(metadata, inner_message);
        }
      } else {
        log("Challenge mismatch (expected: " + std::to_string(expected_challenge) +
                ", got: " + std::to_string(message->header_challenge) + ") for 0x" + toHex(mac_address),
            ESP_LOG_WARN);
      }
      // Remove previous challenge (even on mismatch to prevent brute force)
      _challenges.erase(mac_address);
    } else {
      log("No challenge registered for 0x" + toHex(mac_address) +
              " (challenge received: " + std::to_string(message->header_challenge) + ")",
          ESP_LOG_WARN);
    }

    break;
  }
  case MESSAGE_ID_DISCOVERY_REQUEST_V1: {
    EspNowDiscoveryRequestV1 *message = (EspNowDiscoveryRequestV1 *)data;
    log("Got discovery request from 0x" + toHex(mac_address) + " and sending reply.", ESP_LOG_INFO);
    handleDiscoveryRequest(mac_addr, message->discovery_challenge);
    break;
  }
  case MESSAGE_ID_CHALLENGE_REQUEST_V1: {
    EspNowChallengeRequestV1 *message = (EspNowChallengeRequestV1 *)data;
    auto firmware_version = message->firmware_version;
    log("Got challenge request from 0x" + toHex(mac_address) +
            ", firmware version: " + std::to_string(firmware_version),
        ESP_LOG_INFO);
    handleChallengeRequest(mac_addr, message->challenge_challenge, firmware_version);
    break;
  }

  default:
    log("Received message with unknown id from device with MAC address 0x" + toHex(mac_address) + ". Got id: 0x" +
            toHex(id),
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
      log("Sending firmware update response to 0x" + toHex(mac_address), ESP_LOG_INFO);
      EspNowChallengeFirmwareResponseV1 message;
      message.challenge_challenge = challenge_challenge;
      strncpy(message.wifi_ssid, metadata->wifi_ssid, sizeof(message.wifi_ssid));
      strncpy(message.wifi_password, metadata->wifi_password, sizeof(message.wifi_password));
      strncpy(message.url, metadata->url, sizeof(message.url));
      strncpy(message.md5, metadata->md5, sizeof(message.md5));
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
  log("Sending challenge response to 0x" + toHex(mac_address) + " with challenge " +
          std::to_string(message.header_challenge),
      ESP_LOG_INFO);
  sendMessageToTemporaryPeer(mac_addr, &message, sizeof(EspNowChallengeResponseV1));
}

void EspNowHost::sendMessageToTemporaryPeer(uint8_t *mac_addr, void *message, size_t length) {
  esp_now_peer_info_t peer_info;
  peer_info.ifidx = _wifi_interface == WiFiInterface::AP ? WIFI_IF_AP : WIFI_IF_STA;
  // Channel 0 means "use the current channel which station or softap is on". We should hardcode this to a specific
  // channel so we for sure use same channel on both router and nodes.
  peer_info.channel = 0;
  peer_info.encrypt = false; // Never use esp NOW encryption.
  std::memcpy(peer_info.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

  esp_err_t r = esp_now_add_peer(&peer_info);
  log("esp_now_add_peer failure: ", r);

  r = _crypt.sendMessage(mac_addr, message, length);
  if (r != ESP_OK) {
    log("_crypt.sendMessage() failure: ", r);
  } else {
    log("Message sent OK (not yet delivered)", ESP_LOG_INFO);
  }

  // We are done with the peer.
  r = esp_now_del_peer(mac_addr);
  log("esp_now_del_peer failure: ", r);
}

uint64_t EspNowHost::macToMac(uint8_t *mac_addr) {
  return ((uint64_t)mac_addr[0] << 40) + ((uint64_t)mac_addr[1] << 32) + ((uint64_t)mac_addr[2] << 24) +
         ((uint64_t)mac_addr[3] << 16) + ((uint64_t)mac_addr[4] << 8) + ((uint64_t)mac_addr[5]);
}

void EspNowHost::log(const std::string message, const esp_log_level_t log_level) {
  if (_on_log) {
    _on_log(message, log_level);
  }
}

void EspNowHost::log(const std::string message, const esp_err_t esp_err) {
  if (esp_err != ESP_OK) {
    const char *errstr = esp_err_to_name(esp_err);
    log(message + " " + std::string(errstr), ESP_LOG_ERROR);
  }
}

std::string EspNowHost::toHex(uint64_t i) {
  std::stringstream sstream;
  sstream << std::hex << i;
  return sstream.str();
}