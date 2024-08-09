/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <esp_system.h>
#ifdef SUPLA_DEVICE_ESP32
#include <esp_chip_info.h>
#endif
#include <supla/tools.h>

void deviceSoftwareReset() {
  esp_restart();
}

bool isDeviceSoftwareResetSupported() {
  return true;
}

bool isLastResetSoft() {
  return esp_reset_reason() == ESP_RST_SW;
}

bool Supla::isLastResetPower() {
  return esp_reset_reason() == ESP_RST_POWERON;
}

int Supla::getPlatformId() {
#ifdef SUPLA_DEVICE_ESP32
  esp_chip_info_t chipInfo = {};
  esp_chip_info(&chipInfo);
  switch (chipInfo.model) {
    case CHIP_ESP32:
      return 2;
    case CHIP_ESP32S2:
      return 3;
    case CHIP_ESP32S3:
      return 4;
    case CHIP_ESP32C3:
      return 5;
    case CHIP_ESP32H2:
      return 6;
    case CHIP_ESP32C2:
      return 7;
    default:
      return 0;
  }
#endif
  return 1;  // ESP8266
}
