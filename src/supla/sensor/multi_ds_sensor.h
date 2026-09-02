// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_
#define SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_

#include <supla/sensor/thermometer.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>

#include <stdint.h>
#include <string.h>


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
          handler(handler), subDeviceId(subDeviceId) {
    // Keep the persisted ID separately because useSubDevices=false must leave
    // the protocol Channel::SubDeviceId at 0 for backward compatibility.
    if (useSubDevices) {
      channel.setSubDeviceId(static_cast<uint8_t>(subDeviceId));
    }
    memcpy(address, deviceAddress, 8);
  }

  void onInit() override;
  void iterateAlways() override;
  double getValue() override;

  void saveSensorConfig();
  void purgeConfig() override;

  uint8_t *getAddress() {
    return address;
  }

  int getSubDeviceId() const {
    return subDeviceId;
  }

  bool isOwnerOfSubDeviceId(int subDeviceId) const override {
    return this->subDeviceId == subDeviceId;
  }

  void setDetailsSend(bool send) { detailsSend = send; }
  bool getDetailsSend() { return detailsSend; }

 protected:
  Supla::Sensor::MultiDsHandlerBase *handler;
  uint8_t address[8] = {};

 private:
  int subDeviceId = -1;
  int8_t retryCounter = 0;
  double lastValidValue = TEMPERATURE_NOT_AVAILABLE;
  bool detailsSend = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MULTI_DS_SENSOR_H_
