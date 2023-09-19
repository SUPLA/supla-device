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

#ifndef SRC_SUPLA_SENSOR_THERM_HYGRO_METER_H_
#define SRC_SUPLA_SENSOR_THERM_HYGRO_METER_H_

#include <supla/channel_element.h>
#include <supla/sensor/thermometer_driver.h>

#define HUMIDITY_NOT_AVAILABLE -1

namespace Supla {
namespace Sensor {
class ThermHygroMeter : public ChannelElement {
 public:
  ThermHygroMeter();

  void setRefreshIntervalMs(int intervalMs);

  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void iterateAlways() override;
  bool iterateConnected() override;

  virtual double getTemp();
  virtual double getHumi();

  int16_t getTempInt16();
  int16_t getHumiInt16();

  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void clearChannelConfigChangedFlag();
  void handleChannelConfigFinished() override;

 protected:
  int16_t getConfiguredTemperatureCorrection();
  int16_t getConfiguredHumidityCorrection();
  int16_t readCorrectionFromIndex(int index);
  void setCorrectionAtIndex(int32_t correction, int index);
  virtual void setTemperatureCorrection(int32_t correction);
  virtual void setHumidityCorrection(int32_t correction);
  void applyCorrectionsAndStoreIt(int32_t temperatureCorrection,
                                  int32_t humidityCorrection,
                                  bool local = false);

  uint32_t lastReadTime = 0;
  uint16_t refreshIntervalMs = 10000;
  bool waitForChannelConfigAndIgnoreIt = false;
  bool configFinishedReceived = false;
  bool defaultConfigReceived = false;
  int8_t setChannelStateFlag = 0;
  uint8_t setChannelResult = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_HYGRO_METER_H_
