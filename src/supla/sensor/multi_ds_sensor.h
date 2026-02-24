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

#ifndef SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_
#define SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_

#include <supla/sensor/thermometer.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>

#include <stdint.h>

#include <DallasTemperature.h>
#include <OneWire.h>

namespace Supla {
namespace Sensor {

class MultiDsHandlerBase;

#pragma pack(push, 1)
struct DsSensorConfig {
  uint8_t channelNumber = 0;
  uint8_t address[8] = {};
};
#pragma pack(pop)

class MultiDsSensor : public Thermometer {
 public:
  explicit MultiDsSensor(int subDeviceId,
      uint8_t *deviceAddress, bool useSubDevices,
      Supla::Sensor::MultiDsHandlerBase *handler) :
          subDeviceId(subDeviceId), handler(handler) {
    if (useSubDevices) {
      channel.setSubDeviceId(subDeviceId);
    }
    memcpy(address, deviceAddress, 8);
  }

  void onInit() override;
  void iterateAlways() override;
  double getValue() override;

  void saveSensorConfig();
  void purgeConfig();

  uint8_t *getAddress() {
    return address;
  }

  int getSubDeviceId() {
    return subDeviceId;
  }

  bool isOwnerOfSubDeviceId(int subDeviceId) const override {
    return channel.getSubDeviceId() == subDeviceId;
  }

  void setDetailsSend(bool send) { detailsSend = send; }
  bool getDetailsSend() { return detailsSend; }

 protected:
  Supla::Sensor::MultiDsHandlerBase *handler;
  DeviceAddress address;

 private:
  int subDeviceId = -1;
  int8_t retryCounter = 0;
  double lastValidValue = TEMPERATURE_NOT_AVAILABLE;
  bool detailsSend = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_
