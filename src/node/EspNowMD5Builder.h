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
#ifndef __ESP_NOW_MD5_BUILDER__
#define __ESP_NOW_MD5_BUILDER__

#include <esp_rom_md5.h>
#include <esp_system.h>
#include <string>

class EspNowMD5Builder {
public:
  void begin();
  void add(uint8_t *data, uint16_t len);
  void calculate();
  void getChars(char *output);
  std::string toString();

private:
  md5_context_t _ctx;
  uint8_t _buf[ESP_ROM_MD5_DIGEST_LEN];
};

#endif // __ESP_NOW_MD5_BUILDER__