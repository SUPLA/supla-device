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


#ifndef SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_
#define SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_

#include "multi_ds_handler_base.h"

#include <DallasTemperature.h>
#include <OneWire.h>

namespace Supla {
namespace Sensor {

class MultiDsHandler : public MultiDsHandlerBase {
 public:
  explicit MultiDsHandler(SuplaDeviceClass *sdc, uint8_t pin) :
      MultiDsHandlerBase(sdc, pin), oneWire(pin) {}

  ~MultiDsHandler() {}

  void onInit() override {
    sdc->addFlags(SUPLA_DEVICE_FLAG_CALCFG_SUBDEVICE_PAIRING);
    sdc->setSubdevicePairingHandler(this);

    sensors.setOneWire(&oneWire);
    sensors.begin();
  }

  double getTemperature(const uint8_t *address) override {
    return sensors.getTempC(address);
  }

  int refreshSensorsCount() override {
    oneWire.reset_search();
    sensors.begin();

    return sensors.getDeviceCount();
  }

  bool getSensorAddress(uint8_t *address, int index) override {
    return sensors.getAddress(address, index);
  }

  void requestTemperatures() override {
    sensors.requestTemperatures();
  }

 protected:
  OneWire oneWire;
  DallasTemperature sensors;
};

};  // namespace Sensor
};  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_H_
