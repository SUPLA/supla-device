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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_OTA_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_OTA_H_

#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <supla/device/sw_update.h>

namespace Supla {

class EspIdfOta : public Supla::Device::SwUpdate {
 public:
  friend Supla::Device::SwUpdate *Supla::Device::SwUpdate::Create(
      SuplaDeviceClass *sdc, const char *url);
  ~EspIdfOta();
  void iterate() override;

 protected:
  bool verifyRsaSignature(const esp_partition_t *update_partition, int binSize);
  void fail(const char *);
  void log(const char *);
  EspIdfOta(SuplaDeviceClass *sdc, const char *newUrl);
  esp_http_client_handle_t client = {};
  esp_ota_handle_t updateHandle = 0;
  uint8_t *otaBuffer = nullptr;
  char *httpAgent = nullptr;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_OTA_H_
