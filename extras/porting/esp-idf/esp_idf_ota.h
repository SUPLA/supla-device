// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
      SuplaDeviceClass *sdc, const char *url, Supla::SwUpdateMode mode);
  ~EspIdfOta();
  void iterate() override;

 protected:
  EspIdfOta(SuplaDeviceClass *sdc,
            const char *newUrl,
            Supla::SwUpdateMode mode);

  bool verifyRsaSignature(const esp_partition_t *update_partition, int binSize);
  void fail(const char *);
  void log(const char *);
  esp_http_client_handle_t client = {};
  esp_ota_handle_t updateHandle = 0;
  uint8_t *otaBuffer = nullptr;
  char *httpAgent = nullptr;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_OTA_H_
