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
#include <supla/device/channel_conflict_resolver.h>

#include "multi_ds_sensor.h"

#define MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC 5
#define MULTI_DS_MAX_DEVICES_COUNT 10

namespace Supla {

namespace Sensor {

enum class MultiDsState {
  READY,
  PARING
};

class MultiDsHandlerBase : public Element,
                           public Supla::Device::SubdevicePairingHandler,
                           public Supla::Device::ChannelConflictResolver {
 public:
  explicit MultiDsHandlerBase(SuplaDeviceClass *sdc, uint8_t pin);
  virtual ~MultiDsHandlerBase() = default;

  void iterateAlways() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;

  bool startPairing(Supla::Protocol::SuplaSrpc *srpc,
                   TCalCfg_SubdevicePairingResult *result) override;

  bool onChannelConflictReport(
      uint8_t *channelReport,
      uint8_t channelReportSize,
      bool hasConflictInvalidType,
      bool hasConflictChannelMissingOnServer,
      bool hasConflictChannelMissingOnDevice) override;

  virtual double getTemperature(const uint8_t *address) = 0;

  /**
   * Sets the maximum number of DS18B20 devices handled by this instance.
   *
   * The default and absolute maximum is 10 devices. The value can only
   * be reduced from the default; setting a higher value has no effect.
   *
   * If the limit is exceeded, pairing of additional devices will fail.
   */
  void setMaxDeviceCount(uint8_t count);

  /**
   * Sets the offset for channel number.
   * 
   * Normally paired thermometers will get next free channel number. If you
   * plan thermometeres on specific position in the device, use this method
   * to define which first channel number should be assigned.
   * 
   * If you set the offset you need to garantee, that the amount of channel
   * defined in 'maxDeviceCount' will be available just after offset
   * 
   * Default value is -1 so no offset is used
   */
  void setChannelNumberOffset(uint8_t offset);

  /**
   * Sets the channel number offset for paired thermometers.
   *
   * By default, newly paired thermometers are assigned the next available
   * channel number. If you need thermometers to start at a specific channel
   * position within the device, use this method to define the first channel
   * number to be assigned.
   *
   * When using an offset, you must ensure that at least 'maxDeviceCount'
   * consecutive channel numbers are available starting from the specified offset.
   *
   * The default value is -1, which means no offset is applied.
   */
  void setUseSubDevices(bool useSubDevices);

  /**
   * Sets the timeout for the pairing process.
   *
   * The timeout defines how long the handler waits for DS18B20 devices
   * to be detected and paired after the pairing procedure has started.
   *
   * The default timeout is 5 seconds.
   */
  void setPairingTimeout(uint8_t timeout);

 protected:
  SuplaDeviceClass *sdc = nullptr;
  Supla::Sensor::MultiDsSensor *addDevice(DeviceAddress deviceAddress,
                                          int channelNumber = -1,
                                          int subDeviceId = -1);
  virtual int refreshSensorsCount() = 0;
  virtual void requestTemperatures() = 0;
  virtual bool getSensorAddress(uint8_t *address, int index) = 0;

 private:
  void notifySrpcAboutParingEnd(int pairingResult, const char *name = nullptr);
  void addressToString(char *buffor, uint8_t *address);

  uint8_t pin;
  Supla::Protocol::SuplaSrpc *srpc = nullptr;

  MultiDsState state = MultiDsState::READY;
  MultiDsSensor *sensors[MULTI_DS_MAX_DEVICES_COUNT] = {};

  uint32_t pairingStartTimeMs = 0;
  uint32_t helperTimeMs = 0;
  uint32_t lastBusReadTime = 0;

  uint8_t maxDeviceCount = MULTI_DS_MAX_DEVICES_COUNT;
  uint8_t pairingTimeout = MUTLI_DS_DEFAULT_PAIRING_DURATION_SEC;
  int channelNumberOffset = -1;
  bool useSubDevices = true;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_MULTI_DS_HANDLER_BASE_H_
