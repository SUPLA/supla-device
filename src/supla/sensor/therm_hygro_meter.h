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
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;

  virtual double getTemp();
  virtual double getHumi();

  int16_t getTempInt16();
  int16_t getHumiInt16();

  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override;

  int16_t getConfiguredTemperatureCorrection();
  int16_t getConfiguredHumidityCorrection();

  /**
   * Apply temperature and humidity corrections and store them to Config
   *
   * @param temperatureCorrection temperature correction in 0.1 degree
   * @param humidityCorrection humidity correction in 0.1 %
   * @param local true if correction should be applied locally and then config
   *        will be send to server
   */
  void applyCorrectionsAndStoreIt(int32_t temperatureCorrection,
                                  int32_t humidityCorrection,
                                  bool local = false);

  /**
   * Set whether correction should be applied or not.
   * By default temperature and humidity corrections are applied on Channel
   * level, so raw value is read from sensor, then it is corrected and
   * send to server.
   * However correction may be applied by a sensor device itself. In this
   * case, correciton is send to that device and value read from sensor is
   * already corrected.
   *
   * @param applyCorrections true if correction should be applied by SD
   */
  void setApplyCorrections(bool applyCorrections);

  /**
   * Set minimum and maximum allowed temperature adjustment.
   * Value 0 means Cloud default +- 10.0.
   *
   * @param minMax minimum and maximum allowed temperature adjustment in 0.1
   * degree C. Allowed range: 0 (default), 1..200 (0.1 .. 20.0). I.e. value 50
   * means +- 5.0 degree C
   */
  void setMinMaxAllowedTemperatureAdjustment(int32_t minMax);

  /**
   * Set minimum and maximum allowed humidity adjustment.
   * Value 0 means Cloud default +- 10.0.
   *
   * @param minMax minimum and maximum allowed humidity adjustment in 0.1 %RH.
   * Allowed range: 0 (default), 1..500 (0.1 .. 50.0). I.e. value 50 means
   * +- 5.0 %RH
   */
  void setMinMaxAllowedHumidityAdjustment(int32_t minMax);

 protected:
  int16_t readCorrectionFromIndex(int index);
  void setCorrectionAtIndex(int32_t correction, int index);
  virtual void setTemperatureCorrection(int32_t correction);
  virtual void setHumidityCorrection(int32_t correction);

  uint32_t lastReadTime = 0;
  uint16_t refreshIntervalMs = 10000;

  /**
   * Minimum and maximum allowed temperature adjustment in 0.1 degree C.
   * Value 0 means Cloud default +- 10.0.
   */
  int16_t minMaxAllowedTemperatureAdjustment = 0;

  /**
   * Minimum and maximum allowed humidity adjustment in 0.1 %RH.
   * Value 0 means Cloud default +- 10.0.
   */
  int16_t minMaxAllowedHumidityAdjustment = 0;

  /**
   * Whether correction should be applied by SD
   */
  bool applyCorrections = true;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_HYGRO_METER_H_
