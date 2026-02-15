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

#include <functional>
#include <cstdint>
#include <utility>

#include <DallasTemperature.h>
#include <OneWire.h>
#include <supla/sensor/thermometer_driver.h>
#include <supla/sensor/thermometer.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>

#define DS_SENSOR_CONFIG_KEY "ds_sensor"

namespace Supla {
namespace Sensor {
  
struct DsSensorConfig {
  uint8_t address[8] = {};
};

class MultiDsSensor : public Thermometer {
 public:
  using GetValueFn = std::function<double(const uint8_t* address)>;

  explicit MultiDsSensor(int subDeviceId,
      uint8_t *deviceAddress, GetValueFn valueProvider) :
          subDeviceId(subDeviceId),
          valueProvider(std::move(valueProvider)) {

    channel.setSubDeviceId(subDeviceId);
    channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESTART_SUBDEVICE);
    channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_IDENTIFY_SUBDEVICE);

    memcpy(address, deviceAddress, 8);
  }

  void iterateAlways() override;
  double getValue() override;

  void saveSensorConfig();

  uint8_t *getAddress() {
    return address;
  }

  int getSubDeviceId() {
    return subDeviceId;
  }

  bool isOwnerOfSubDeviceId(int subDeviceId) const override {
    return channel.getSubDeviceId() == subDeviceId;
  }

 protected:
  GetValueFn valueProvider;
  DeviceAddress address;

 private:
  int subDeviceId = -1;
  int8_t retryCounter = 0;
  uint32_t lastReadTime = 0;
  double lastValidValue = TEMPERATURE_NOT_AVAILABLE;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_
