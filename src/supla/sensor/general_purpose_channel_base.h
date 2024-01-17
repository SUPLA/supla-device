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

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_CHANNEL_BASE_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_CHANNEL_BASE_H_

#include <supla/channel_element.h>

namespace Supla {
namespace Sensor {

class MeasurementDriver;

class GeneralPurposeChannelBase : public ChannelElement {
 public:
#pragma pack(push, 1)
  struct GPMCommonConfig {
    int32_t divider = 0;
    int32_t multiplier = 0;
    int64_t added = 0;
    uint8_t precision = 0;
    uint8_t noSpaceAfterValue = 0;
    uint8_t keepHistory = 0;
    uint8_t chartType = 0;
    uint16_t refreshIntervalMs = 0;
    char unitBeforeValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
    char unitAfterValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
  };
#pragma pack(pop)

  explicit GeneralPurposeChannelBase(MeasurementDriver *driver = nullptr);
  virtual ~GeneralPurposeChannelBase() = default;


  // For custom sensor, please provide either class that inherits from
  // Supla::Sensor::MeasurementDriver or override below getValue() method
  virtual double getValue();

  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void iterateAlways() override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result) override;
  void fillChannelConfig(void *channelConfig, int *size) override;

  // default parameters should be configured in software. GPM doesn't store
  // it by itself
  void setDefaultValueDivider(int32_t divider);
  void setDefaultValueMultiplier(int32_t multiplier);
  void setDefaultValueAdded(int64_t added);
  void setDefaultValuePrecision(uint8_t precision);
  void setDefaultUnitBeforeValue(const char *unit);
  void setDefaultUnitAfterValue(const char *unit);

  int32_t getDefaultValueDivider() const;
  int32_t getDefaultValueMultiplier() const;
  int64_t getDefaultValueAdded() const;
  uint8_t getDefaultValuePrecision() const;
  void getDefaultUnitBeforeValue(char *unit);
  void getDefaultUnitAfterValue(char *unit);

  uint16_t getRefreshIntervalMs() const;
  int32_t getValueDivider() const;
  int32_t getValueMultiplier() const;
  int64_t getValueAdded() const;
  uint8_t getValuePrecision() const;
  void getUnitBeforeValue(char *unit) const;
  void getUnitAfterValue(char *unit) const;
  uint8_t getNoSpaceAfterValue() const;
  uint8_t getKeepHistory() const;
  uint8_t getChartType() const;

  void setRefreshIntervalMs(int intervalMs, bool local = true);
  void setValueDivider(int32_t divider, bool local = true);
  void setValueMultiplier(int32_t multiplier, bool local = true);
  void setValueAdded(int64_t added, bool local = true);
  void setValuePrecision(uint8_t precision, bool local = true);
  void setUnitBeforeValue(const char *unit, bool local = true);
  void setUnitAfterValue(const char *unit, bool local = true);
  void setNoSpaceAfterValue(uint8_t noSpaceAfterValue, bool local = true);
  void setKeepHistory(uint8_t keepHistory, bool local = true);
  void setChartType(uint8_t chartType, bool local = true);

 protected:
  void saveConfig();
  void setChannelRefreshIntervalMs(uint16_t intervalMs);

  MeasurementDriver *driver = nullptr;
  uint16_t refreshIntervalMs = 10000;
  uint32_t lastReadTime = 0;

  int32_t defaultValueDivider = 0;  // 0.001 units; 0 is considered as 1
  int32_t defaultValueMultiplier = 0;  // 0.001 units; 0 is considered as 1
  int64_t defaultValueAdded = 0;  // 0.001 units
  uint8_t defaultValuePrecision = 0;  // 0 - 4 decimal points
  // Default unit (before value) - UTF8 including the terminating null byte '\0'
  char defaultUnitBeforeValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
  // Default unit (after value) - UTF8 including the terminating null byte '\0'
  char defaultUnitAfterValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};

  GPMCommonConfig commonConfig = {};
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_CHANNEL_BASE_H_
