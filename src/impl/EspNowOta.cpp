#include "EspNowOta.h"

#include "EspNowMD5Builder.h"
#include <esp_app_format.h>
#include <esp_crt_bundle.h>
#include <esp_flash_partitions.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_tls.h>
#include <esp_tls_crypto.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
#include <spi_flash_mmap.h>
#endif

// Inspired by Ardunio Updater.h

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

#define SPI_SECTORS_PER_BLOCK 16 // usually large erase block is 32k/64k
#define SPI_FLASH_BLOCK_SIZE (SPI_SECTORS_PER_BLOCK * SPI_FLASH_SEC_SIZE)

EspNowOta::EspNowOta(OnLog on_log) : _on_log(on_log) { _wifi_event_group = xEventGroupCreate(); }

void EspNowOta::wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  EspNowOta *wrapper = (EspNowOta *)arg;
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (wrapper->_wifi_retry_number < wrapper->_wifi_num_retries) {
      esp_wifi_connect();
      wrapper->_wifi_retry_number++;
      wrapper->log("retry to connect to the AP", ESP_LOG_INFO);
    } else {
      xEventGroupSetBits(wrapper->_wifi_event_group, WIFI_FAIL_BIT);
    }
    wrapper->log("connect to the AP failed", ESP_LOG_WARN);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    memcpy(&wrapper->_ip_addr, &event->ip_info.ip, sizeof(esp_ip4_addr_t));
    wrapper->_wifi_retry_number = 0;
    xEventGroupSetBits(wrapper->_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

bool EspNowOta::connectToWiFi(const char *ssid, const char *password, unsigned long connect_timeout_ms,
                              uint16_t retries) {

  _wifi_num_retries = retries;

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta = esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;

  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, this, &instance_any_id));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, this, &instance_got_ip));

  wifi_config_t wifi_config = {};
  strncpy((char *)wifi_config.sta.ssid, ssid, 31);
  strncpy((char *)wifi_config.sta.password, password, 63);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  log("wifi_init_sta finished.", ESP_LOG_INFO);

  /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
   * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
  EventBits_t bits = xEventGroupWaitBits(_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
                                         connect_timeout_ms / portTICK_PERIOD_MS);

  /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
   * happened. */
  if (bits & WIFI_CONNECTED_BIT) {
    log("connected to SSID: " + std::string(ssid), ESP_LOG_INFO);
    return true;
  } else if (bits & WIFI_FAIL_BIT) {
    log("Failed to connect to SSID: " + std::string(ssid), ESP_LOG_INFO);
  } else {
    log("Got unexpected event", ESP_LOG_ERROR);
  }

  // On failure, cleanup.
  esp_netif_destroy_default_wifi(sta);
  esp_event_loop_delete_default();
  esp_netif_deinit();
  return false;
}

bool EspNowOta::updateFrom(std::string &url, std::string md5hash) {
  auto *partition = esp_ota_get_next_update_partition(NULL);
  if (!partition) {
    log("Unable to find OTA partition", ESP_LOG_ERROR);
    return false;
  }
  log("Found partition " + std::string(partition->label), ESP_LOG_INFO);

  if (!md5hash.empty() && md5hash.length() != 32) {
    log("MD5 is not correct length. Leave empty for no MD5 checksum verification. Expected length: 32, got " +
            std::to_string(md5hash.length()),
        ESP_LOG_ERROR);
    return false;
  }

  return downloadAndWriteToPartition(partition, url, md5hash);
}

esp_err_t EspNowOta::httpEventHandler(esp_http_client_event_t *evt) {
  EspNowOta *esp_now_ota = (EspNowOta *)evt->user_data;

  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    esp_now_ota->log("HTTP_EVENT_ERROR", ESP_LOG_VERBOSE);
    break;
  case HTTP_EVENT_ON_CONNECTED:
    esp_now_ota->log("HTTP_EVENT_ON_CONNECTED", ESP_LOG_VERBOSE);
    break;
  case HTTP_EVENT_HEADER_SENT:
    esp_now_ota->log("HTTP_EVENT_HEADER_SENT", ESP_LOG_VERBOSE);
    break;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
  case HTTP_EVENT_REDIRECT:
    esp_now_ota->log("HTTP_EVENT_REDIRECT", ESP_LOG_VERBOSE);
    break;
#endif
  case HTTP_EVENT_ON_HEADER:
    esp_now_ota->log("HTTP_EVENT_ON_HEADER, key=" + std::string(evt->header_key) +
                         ", value=" + std::string(evt->header_value),
                     ESP_LOG_INFO);
    break;
  case HTTP_EVENT_ON_DATA:
    esp_now_ota->log("HTTP_EVENT_ON_DATA, len=" + std::to_string(evt->data_len), ESP_LOG_VERBOSE);
    break;
  case HTTP_EVENT_ON_FINISH:
    esp_now_ota->log("HTTP_EVENT_ON_FINISH", ESP_LOG_INFO);
    break;
  case HTTP_EVENT_DISCONNECTED:
    esp_now_ota->log("HTTP_EVENT_DISCONNECTED", ESP_LOG_INFO);
    break;
  }

  return ESP_OK;
}

bool EspNowOta::downloadAndWriteToPartition(const esp_partition_t *partition, std::string &url, std::string &md5hash) {

  char *buffer = (char *)malloc(SPI_FLASH_SEC_SIZE);

  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.user_data = this;
  config.event_handler = httpEventHandler;
  config.buffer_size = SPI_FLASH_SEC_SIZE;
  /*#if PIOFRAMEWORK == "arduino"
    config.crt_bundle_attach = arduino_esp_crt_bundle_attach;
  #elif PIOFRAMEWORK == "espidf"
    config.crt_bundle_attach = esp_crt_bundle_attach;
  #else
  #error "PIOFRAMEWORK is either not defined in platformio.ini or framework is not supported."
  #endif*/
  esp_http_client_handle_t client = esp_http_client_init(&config);

  log("Using URL " + url, ESP_LOG_INFO);
  esp_http_client_set_method(client, HTTP_METHOD_GET);
  esp_http_client_set_header(client, "Accept", "*/*");
  esp_http_client_set_timeout_ms(client, HTTP_TIMEOUT_MS);

  bool success = false;
  esp_err_t r = esp_http_client_open(client, 0);
  if (r == ESP_OK) {
    esp_http_client_fetch_headers(client);
    auto status_code = esp_http_client_get_status_code(client);
    auto content_length = esp_http_client_get_content_length(client);
    log("Http status code " + std::to_string(status_code) + " with content length " + std::to_string(content_length),
        ESP_LOG_INFO);

    if (status_code == 200) {
      if (content_length > partition->size) {
        log("Content length " + std::to_string(content_length) + " is larger than partition size " +
                std::to_string(partition->size),
            ESP_LOG_ERROR);
      } else {
        success = writeStreamToPartition(partition, client, content_length, md5hash);
      }
    } else {
      log("Got non 200 status code: " + std::to_string(status_code), ESP_LOG_ERROR);
    }

  } else {
    const char *errstr = esp_err_to_name(r);
    log("Failed to open HTTP connection: " + std::string(errstr), ESP_LOG_ERROR);
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  if (buffer != nullptr) {
    free(buffer);
  }
  return success;
}

int EspNowOta::fillBuffer(esp_http_client_handle_t client, char *buffer, size_t buffer_size) {
  int total_read = 0;
  while (total_read < buffer_size) {
    int read = esp_http_client_read(client, buffer + total_read, buffer_size - total_read);
    if (read <= 0) {
      if (esp_http_client_is_complete_data_received(client)) {
        return total_read;
      } else {
        log("Failed to fill buffer, read zero and not complete.", ESP_LOG_ERROR);
        return -1;
      }
    }
    total_read += read;
  }
  return total_read;
}

bool EspNowOta::writeStreamToPartition(const esp_partition_t *partition, esp_http_client_handle_t client,
                                       uint32_t content_length, std::string &md5hash) {
  char *buffer = (char *)malloc(SPI_FLASH_SEC_SIZE);
  if (buffer == nullptr) {
    log("Failed to allocate buffer of size " + std::to_string(SPI_FLASH_SEC_SIZE), ESP_LOG_ERROR);
    return false;
  }

  uint8_t skip_buffer[ENCRYPTED_BLOCK_SIZE];

  EspNowMD5Builder md5;
  md5.begin();

  int bytes_read = 0;
  while (bytes_read < content_length) {
    int bytes_filled = fillBuffer(client, buffer, SPI_FLASH_SEC_SIZE);
    if (bytes_filled < 0) {
      log("Unable to fill buffer", ESP_LOG_ERROR);
      free(buffer);
      return false;
    }

    log("Filled buffer with: " + std::to_string(bytes_filled), ESP_LOG_INFO);

    // Special start case
    // Check start if contains the magic byte.
    uint8_t skip = 0;
    if (bytes_read == 0) {
      if (buffer[0] != ESP_IMAGE_HEADER_MAGIC) {
        log("Start of firwmare does not contain magic byte", ESP_LOG_ERROR);
        free(buffer);
        return false;
      }

      // Stash the first 16/ENCRYPTED_BLOCK_SIZE bytes of data and set the offset so they are
      // not written at this point so that partially written firmware
      // will not be bootable
      memcpy(skip_buffer, buffer, sizeof(skip_buffer));
      skip += sizeof(skip_buffer);
    }

    // Normal case - write buffer
    if (!writeBufferToPartition(partition, bytes_read, buffer, bytes_filled, skip)) {
      log("Failed to write buffer to partition", ESP_LOG_ERROR);
      free(buffer);
      return false;
    }

    md5.add((uint8_t *)buffer, (uint16_t)bytes_filled);

    // If this is the end, finish up.
    if (bytes_filled != SPI_FLASH_SEC_SIZE) {
      log("This is the end", ESP_LOG_INFO);

      if (!md5hash.empty()) {
        md5.calculate();
        if (md5hash != md5.toString()) {
          log("MD5 checksum verification failed.", ESP_LOG_ERROR);
          free(buffer);
          return false;
        } else {
          log("MD5 checksum correct.", ESP_LOG_INFO);
        }
      }

      auto r = esp_partition_write(partition, 0, (uint32_t *)skip_buffer, ENCRYPTED_BLOCK_SIZE);
      if (r != ESP_OK) {
        log("Failed to enable partition", r);
        free(buffer);
        return false;
      }

      r = partitionIsBootable(partition);
      if (r != ESP_OK) {
        log("Partition is not bootable", r);
        free(buffer);
        return false;
      }

      r = esp_ota_set_boot_partition(partition);
      if (r != ESP_OK) {
        free(buffer);
        log("Failed to set partition as bootable", r);
        return false;
      }
    }

    bytes_read += bytes_filled;
    vTaskDelay(0); // Yield/reschedule
  }

  free(buffer);
  return true;
}

bool EspNowOta::writeBufferToPartition(const esp_partition_t *partition, size_t bytes_written, char *buffer,
                                       size_t buffer_size, uint8_t skip) {

  size_t offset = partition->address + bytes_written;
  bool block_erase =
      (buffer_size - bytes_written >= SPI_FLASH_BLOCK_SIZE) &&
      (offset % SPI_FLASH_BLOCK_SIZE == 0); // if it's the block boundary, than erase the whole block from here
  bool part_head_sectors = partition->address % SPI_FLASH_BLOCK_SIZE &&
                           offset < (partition->address / SPI_FLASH_BLOCK_SIZE + 1) *
                                        SPI_FLASH_BLOCK_SIZE; // sector belong to unaligned partition heading block
  bool part_tail_sectors = offset >= (partition->address + buffer_size) / SPI_FLASH_BLOCK_SIZE *
                                         SPI_FLASH_BLOCK_SIZE; // sector belong to unaligned partition tailing block
  if (block_erase || part_head_sectors || part_tail_sectors) {
    esp_err_t r =
        esp_partition_erase_range(partition, bytes_written, block_erase ? SPI_FLASH_BLOCK_SIZE : SPI_FLASH_SEC_SIZE);
    if (r != ESP_OK) {
      log("Failed to erase range.", r);
      return false;
    }
  }

  // try to skip empty blocks on unecrypted partitions
  if (partition->encrypted || checkDataInBlock((uint8_t *)buffer + skip / sizeof(uint32_t), bytes_written - skip)) {
    auto r = esp_partition_write(partition, bytes_written + skip, (uint32_t *)buffer + skip / sizeof(uint32_t),
                                 buffer_size - skip);
    if (r != ESP_OK) {
      log("Failed to write range.", r);
      return false;
    }
  }

  return true;
}

esp_err_t EspNowOta::partitionIsBootable(const esp_partition_t *partition) {
  uint8_t buf[ENCRYPTED_BLOCK_SIZE];
  if (!partition) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t r = esp_partition_read(partition, 0, (uint32_t *)buf, ENCRYPTED_BLOCK_SIZE);
  if (r != ESP_OK) {
    return r;
  }

  if (buf[0] != ESP_IMAGE_HEADER_MAGIC) {
    return ESP_ERR_INVALID_CRC;
  }
  return ESP_OK;
}

void EspNowOta::log(const std::string message, const esp_log_level_t log_level) {
  if (_on_log) {
    _on_log(message, log_level);
  }
}

void EspNowOta::log(const std::string message, const esp_err_t esp_err) {
  if (esp_err != ESP_OK) {
    const char *errstr = esp_err_to_name(esp_err);
    log(message + " " + std::string(errstr), ESP_LOG_ERROR);
  }
}

bool EspNowOta::checkDataInBlock(const uint8_t *data, size_t len) {
  // check 32-bit aligned blocks only
  if (!len || len % sizeof(uint32_t))
    return true;

  size_t dwl = len / sizeof(uint32_t);

  do {
    if (*(uint32_t *)data ^ 0xffffffff) // for SPI NOR flash empty blocks are all one's, i.e. filled with 0xff byte
      return true;

    data += sizeof(uint32_t);
  } while (--dwl);
  return false;
}