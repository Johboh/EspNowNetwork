#ifndef __FIRMWARE_KICKER_H__
#define __FIRMWARE_KICKER_H__

#include "IFirmwareChecker.h"
#include "IFirmwareKicker.h"
#include <cstdint>
#include <esp_err.h>
#include <esp_http_server.h>

using OnLog = IFirmwareKicker::OnLog;

/**
 * @brief A HTTP server listenting on GET calls to /kicker with a device type and (optionally) hardware query
 * parameter. Once received, will call Firmware Checker to check if there is a new firmware version available for the
 * type & hardware combo.
 * Example: GET /kicker?type=foot_pedal&hardware=esp32c3
 */
class FirmwareKicker : public IFirmwareKicker {
public:
  FirmwareKicker(IFirmwareChecker &firmware_checker, uint16_t port);

public:
  // Call to start the server.
  void start();

  /**
   * @brief Register log callback.
   */
  void addOnLog(OnLog on_log) override { _on_log.push_back(on_log); }

private:
  static esp_err_t httpGetHandler(httpd_req_t *req);
  void log(const std::string message, const esp_log_level_t log_level);

private:
  uint16_t _port;
  std::vector<OnLog> _on_log;
  IFirmwareChecker &_firmware_checker;
};

#endif // __FIRMWARE_KICKER_H__