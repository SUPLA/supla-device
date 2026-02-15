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

#ifndef SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_BASE_H_
#define SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_BASE_H_

#include <SuplaDevice.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/local_action.h>
#include <supla/device/subdevice_pairing_handler.h>

#include "multi_ds_sensor.h"

#define MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC 10
#define MULTI_DS_MAX_DEVICES_COUNT 10

namespace Supla {

namespace Sensor {

enum class MultiDsState {
  READY,
  PARING
};

class MultiDsHandlerBase : public Element,
                           public Supla::Device::SubdevicePairingHandler {

 public:
  explicit MultiDsHandlerBase(SuplaDeviceClass *sdc, uint8_t pin);
  virtual ~MultiDsHandlerBase() = default;

  void iterateAlways() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;

  bool startPairing(Supla::Protocol::SuplaSrpc *srpc,
                   TCalCfg_SubdevicePairingResult *result) override;

  virtual double getTemperature(const uint8_t *address) = 0;
  virtual int refreshSensorsCount() = 0;
  virtual bool getSensorAddress(uint8_t *address, int index) = 0;
  virtual void requestTemperatures() = 0;

  void setMaxDeviceCount(uint8_t count);
  void setChannelNumberOffset(uint8_t offset);

 protected:
  SuplaDeviceClass *sdc = nullptr;
  Supla::Sensor::MultiDsSensor *addDevice(DeviceAddress deviceAddress,
                                          int subDeviceId = -1);

 private:
  void notifySrpcAboutParingEnd(int pairingResult, const char *name = nullptr);
  void addressToString(char *buffor, uint8_t *address);

  uint8_t pin;
  Supla::Protocol::SuplaSrpc *srpc = nullptr;

  MultiDsState state = MultiDsState::READY;
  MultiDsSensor *devices[MULTI_DS_MAX_DEVICES_COUNT] = {};

  uint32_t pairingStartTimeMs = 0;
  uint32_t helperTimeMs = 0;
  uint32_t lastBusReadTime = 0;

  uint8_t maxDeviceCount = MULTI_DS_MAX_DEVICES_COUNT;
  uint8_t channelNumberOffset = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_BASE_H_
