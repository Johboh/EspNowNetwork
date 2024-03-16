#ifndef __FIRMWARE_CHECKER_UTILS_H__
#define __FIRMWARE_CHECKER_UTILS_H__

#include <esp_http_client.h>
#include <memory>
#include <optional>
#include <string>

#define HTTP_REMOTE_TIMEOUT_MS 15000

namespace FirmwareCheckerUtils {

/**
 * @brief Given an URL, get the content of the file as a string.
 *
 * Very little error handling.
 *
 * @param url the URL to get the string content for.
 * @return content as a string, or std::nullopt.
 */
static std::optional<std::string> getContentStringForUrl(std::string &url) {

  const int buffer_size = 1024; // Should be enough for everyone.
  std::unique_ptr<char[]> buffer(new (std::nothrow) char[buffer_size]);
  if (buffer == nullptr) {
    // Oh noes!
    return std::nullopt;
  }
  std::optional<std::string> content_str = std::nullopt;

  esp_http_client_config_t config = {};
  config.url = url.c_str();
  config.buffer_size = buffer_size;
  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_GET);
  esp_http_client_set_header(client, "Accept", "*/*");
  esp_http_client_set_timeout_ms(client, HTTP_REMOTE_TIMEOUT_MS);

  esp_err_t r = esp_http_client_open(client, 0);
  if (r == ESP_OK) {
    esp_http_client_fetch_headers(client);
    auto status_code = esp_http_client_get_status_code(client);

    if (status_code == 200) {
      int total_read = 0;
      while (total_read < buffer_size) {
        int read = esp_http_client_read(client, buffer.get() + total_read, buffer_size - total_read);
        if (read <= 0) { // Done or error
          if (esp_http_client_is_complete_data_received(client)) {
            content_str = std::string(buffer.get(), total_read);
          }
          break;
        }
        total_read += read;
      }
    }
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);

  return content_str;
}

}; // namespace FirmwareCheckerUtils

#endif // __FIRMWARE_CHECKER_UTILS_H__