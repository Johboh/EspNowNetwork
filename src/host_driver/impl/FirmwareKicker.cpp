#include "FirmwareKicker.h"
#include <esp_log.h>
#include <memory>

FirmwareKicker::FirmwareKicker(IFirmwareChecker &firmware_checker, uint16_t port)
    : _port(port), _firmware_checker(firmware_checker) {}

void FirmwareKicker::start() {
  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  // Must use unique unique internal UDP port in case of several HTTP servers on this host. OK to wrap.
  config.ctrl_port = config.ctrl_port + _port;
  config.server_port = _port;
  config.lru_purge_enable = true;

  auto err = httpd_start(&server, &config);
  if (err != ESP_OK) {
    log("Error starting server: " + std::string(esp_err_to_name(err)), ESP_LOG_ERROR);
    return;
  }

  httpd_uri_t kicker_path = {
      .uri = "/kicker",
      .method = HTTP_GET,
      .handler = httpGetHandler,
      .user_ctx = this,
  };

  err = httpd_register_uri_handler(server, &kicker_path);
  if (err != ESP_OK) {
    log("Failed ro register handler: " + std::string(esp_err_to_name(err)), ESP_LOG_ERROR);
    return;
  }

  log("Started kicker server at " + std::to_string(_port), ESP_LOG_INFO);
}

esp_err_t FirmwareKicker::httpGetHandler(httpd_req_t *req) {
  FirmwareKicker *_this = (FirmwareKicker *)req->user_ctx;

  _this->log("Got kicked! Parsing arguments...", ESP_LOG_INFO);

  auto required_size = httpd_req_get_url_query_len(req) + 1; // plus one for null termination
  required_size = std::min(required_size, (size_t)1024);     // Be reasonable, limit to prevent overflow.

  std::unique_ptr<char[]> query_buffer(new (std::nothrow) char[required_size]);

  esp_err_t err = httpd_req_get_url_query_str(req, query_buffer.get(), required_size);
  if (err == ESP_OK) {
    char param[255] = {0};
    err = httpd_query_key_value(query_buffer.get(), "device", param, sizeof(param));
    if (err != ESP_OK) {
      _this->log("Failed to parse device from query: " + std::string(esp_err_to_name(err)), ESP_LOG_WARN);
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse device from query");
      return ESP_OK;
    }
    std::string device(param);

    err = httpd_query_key_value(query_buffer.get(), "hardware", param, sizeof(param));
    std::optional<std::string> hardware = std::nullopt;
    if (err == ESP_OK) {
      hardware = std::string(param);
    }

    _this->log("Got kicked with device: " + device + " and hardware: " + (hardware ? hardware.value() : "<absent>"),
               ESP_LOG_INFO);
    _this->_firmware_checker.checkNow(device, hardware);
  } else {
    _this->log("Failed to get query string:" + std::string(esp_err_to_name(err)), ESP_LOG_ERROR);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to get query string");
    return ESP_OK;
  }

  httpd_resp_sendstr(req, "OK");
  return ESP_OK;
}

void FirmwareKicker::log(const std::string message, const esp_log_level_t log_level) {
  for (auto &on_log : _on_log) {
    if (on_log) {
      on_log(message, log_level);
    }
  }
}