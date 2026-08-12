// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_MULTI_DS_HANDLER_ESP_IDF_H_
#define EXTRAS_PORTING_ESP_IDF_MULTI_DS_HANDLER_ESP_IDF_H_

#include <stdint.h>

#include <supla/sensor/multi_ds_handler_base.h>

namespace Supla {
namespace Sensor {

class MultiDsHandlerEspIdf : public MultiDsHandlerBase {
 public:
  explicit MultiDsHandlerEspIdf(SuplaDeviceClass *sdc, uint8_t pin);
  ~MultiDsHandlerEspIdf() override;

  void onInit() override;

 protected:
  int refreshSensorsCount() override;
  void requestTemperatures() override;
  bool getSensorAddress(uint8_t *address, int index) override;
  double getTemperature(const uint8_t *address) override;

 private:
  class Impl;
  Impl *impl = nullptr;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_MULTI_DS_HANDLER_ESP_IDF_H_
