#ifndef __ESP_NOW_OTA_H__
#define __ESP_NOW_OTA_H__

#include <cstring>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <functional>
#include <string>

#define HTTP_TIMEOUT_MS 15000
#define ENCRYPTED_BLOCK_SIZE 16

class EspNowOta {
public:
  /**
   * @brief Callback when the node want to log something.
   *
   * This doesn't need to be implemented. But can be used to print debug information to serial.
   *
   * @param message the log message to log.
   * @param log_level the severity of the log.
   */
  typedef std::function<void(const std::string message, const esp_log_level_t log_level)> OnLog;

  EspNowOta(OnLog on_log = {});

  /**
   * @brief Connect to wifi.
   */
  bool connectToWiFi(const char *ssid, const char *password, uint16_t retries);

  /**
   * @brief Try to update firmware from the given URL.
   * WiFi needs to be established first.
   */
  bool updateFrom(std::string &url, std::string md5hash = "");

private:
  int fillBuffer(esp_http_client_handle_t client, char *buffer, size_t buffer_size);
  bool downloadAndWriteToPartition(const esp_partition_t *partition, std::string &url, std::string &md5hash);
  bool writeStreamToPartition(const esp_partition_t *partition, esp_http_client_handle_t client,
                              uint32_t content_length, std::string &md5hash);
  bool writeBufferToPartition(const esp_partition_t *partition, size_t bytes_written, char *buffer, size_t buffer_size,
                              uint8_t skip);

  esp_err_t partitionIsBootable(const esp_partition_t *partition);
  bool checkDataInBlock(const uint8_t *data, size_t len);

  void log(const std::string message, const esp_log_level_t log_level);
  void log(const std::string message, const esp_err_t esp_err);

  static esp_err_t httpEventHandler(esp_http_client_event_t *evt);
  static void wifiEventHandler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

private:
  OnLog _on_log;
  esp_ip4_addr_t _ip_addr;
  uint16_t _wifi_num_retries = 0;
  uint16_t _wifi_retry_number = 0;
  EventGroupHandle_t _wifi_event_group;

private:
  uint8_t *_buffer;
};

#endif // __ESP_NOW_OTA_H__