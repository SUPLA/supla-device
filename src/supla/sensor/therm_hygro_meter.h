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
  void fillChannelConfig(void *channelConfig, int *size) override;

  virtual double getTemp();
  virtual double getHumi();

  int16_t getTempInt16();
  int16_t getHumiInt16();

  uint8_t applyChannelConfig(TSD_ChannelConfig *result) override;

  int16_t getConfiguredTemperatureCorrection();
  int16_t getConfiguredHumidityCorrection();
  void applyCorrectionsAndStoreIt(int32_t temperatureCorrection,
                                  int32_t humidityCorrection,
                                  bool local = false);

 protected:
  int16_t readCorrectionFromIndex(int index);
  void setCorrectionAtIndex(int32_t correction, int index);
  virtual void setTemperatureCorrection(int32_t correction);
  virtual void setHumidityCorrection(int32_t correction);

  uint32_t lastReadTime = 0;
  uint16_t refreshIntervalMs = 10000;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_HYGRO_METER_H_
