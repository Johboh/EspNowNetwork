/*
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the esp8266 core for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "EspNowMD5Builder.h"
#include <cstring>

void EspNowMD5Builder::begin() {
  std::memset(_buf, 0x00, ESP_ROM_MD5_DIGEST_LEN);
  esp_rom_md5_init(&_ctx);
}

void EspNowMD5Builder::add(uint8_t *data, uint16_t len) { esp_rom_md5_update(&_ctx, data, len); }

void EspNowMD5Builder::calculate() { esp_rom_md5_final(_buf, &_ctx); }

void EspNowMD5Builder::getChars(char *output) {
  for (uint8_t i = 0; i < ESP_ROM_MD5_DIGEST_LEN; i++) {
    sprintf(output + (i * 2), "%02x", _buf[i]);
  }
}

std::string EspNowMD5Builder::toString() {
  char out[(ESP_ROM_MD5_DIGEST_LEN * 2) + 1];
  getChars(out);
  return std::string(out);
}