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

/**
 * Base class for General Purpose Measurement (GPM/KPOP) and
 * General Purpose Meter (GPM/KLOP)
 *
 * GPM can have either MeasurementDriver provided which is used to obtain
 * value for a channel, or derived class can be used, which implements
 * getValue method.
 */
class GeneralPurposeChannelBase : public ChannelElement {
 public:
#pragma pack(push, 1)
  struct GPMCommonConfig {
    int32_t divider = 0;
    int32_t multiplier = 0;
    int64_t added = 0;
    uint8_t precision = 0;
    uint8_t noSpaceBeforeValue = 0;
    uint8_t noSpaceAfterValue = 0;
    uint8_t keepHistory = 0;
    uint8_t chartType = 0;
    uint16_t refreshIntervalMs = 0;
    char unitBeforeValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
    char unitAfterValue[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
  };
#pragma pack(pop)

  /**
   * Constructor
   *
   * @param driver (optional) pointer to MeasurementDriver instance.
   * @param addMemoryVariableDriver (optional) if true, MemoryVariableDriver
   *                                will be added to channel, which works as
   *                                virtual sensors.
   */
  explicit GeneralPurposeChannelBase(MeasurementDriver *driver = nullptr,
      bool addMemoryVariableDriver = true);

  /**
   * Destructor
   */
  virtual ~GeneralPurposeChannelBase();

  /**
   * Returns calculated value, which is result of the following operations:
   * - value divider
   * - value multiplier
   * - value added
   *
   * @return calculated value
   */
  double getCalculatedValue();

  /**
   * Returns formatted value as char array (including units, precision, etc.)
   *
   * @param result pointer to char array
   * @param maxSize size of provided char array
   */
  void getFormattedValue(char *result, int maxSize);

  /**
   * Method used to obtain new value for channel.
   * For custom sensor, please provide either class that inherits from
   * Supla::Sensor::MeasurementDriver or override below getValue() method
   *
   * @return new raw measurment value
   */
  virtual double getValue();

  /**
   * Method used to set new value for channel with driver which accepts
   * MeasurementDriver::setValue() method.
   *
   * @param value
   */
  virtual void setValue(const double &value);

  /**
   * Supla::Element::onInit() - called by SuplaDeviceClass::begin() during
   * initialization.
   */
  void onInit() override;

  /**
   * Supla::Element::onLoadConfig() - called by SuplaDeviceClass::loadConfig()
   * during initialization.
   *
   * @param sdc pointer to SuplaDeviceClass
   */
  void onLoadConfig(SuplaDeviceClass *sdc) override;

  /**
   * Supla::Element::iterateAlways() - called by SuplaDeviceClass::iterate()
   */
  void iterateAlways() override;

  /**
   * Applies new Channel Config (i.e. received from Supla Server)
   *
   * @param result pointer to TSD_ChannelConfig which should contain
   *               TChannelConfig_GeneralPurposeMeasurement or
   *               TChannelConfig_GeneralPurposeMeter class with configuration.
   * @param local set to true if method is called locally by device. Use false
   *              if method is called as a result of Supla Server message.
   *
   * @return result of operation
   */
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override;

  /**
   * Fills Channel Config
   *
   * @param channelConfig pointer to TSD_ChannelConfig to be filled.
   * @param size written config size is stored here.
   */
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

  /** @name Setters for default values.
   *  Default parameters should be configured in software. GPM doesn't store
   *  it by itself.
   *  Defualt values are applied by server as first default configuration when
   *  channel is registered for the first time and configuration for those
   *  parameters is not set.
   *  If default value stored on server is different from current channel's
   *  configuration, it will be applied. However current configuration value
   *  will not be overwritten. Use "reset to defaults" button on Supla Cloud
   *  to reset configuration to default values.
   */
  ///@{
  /**
   * Sets default value divider.
   *
   * @param divider value in 0.001 units. 0 is ignored
   */
  void setDefaultValueDivider(int32_t divider);

  /**
   * Sets default value multiplier
   *
   * @param multiplier value in 0.001 units. 0 is ignored
   */
  void setDefaultValueMultiplier(int32_t multiplier);

  /**
   * Sets default value added.
   *
   * @param added value in 0.001 units.
   */
  void setDefaultValueAdded(int64_t added);

  /**
   * Sets default precision (number of decimal places)
   *
   * @param precision value in range 0-4
   */
  void setDefaultValuePrecision(uint8_t precision);

  /**
   * Sets default unit which is displayed before value.
   *
   * @param unit before value - max 15 bytes including terminating null byte
   */
  void setDefaultUnitBeforeValue(const char *unit);

  /**
   * Sets default unit which is displayed after value.
   *
   * @param unit after value - max 15 bytes including terminating null byte
   */
  void setDefaultUnitAfterValue(const char *unit);
  ///@}

  /** @name Getters for default values.
   */
  ///@{
  /**
   * Returns default value divider
   *
   * @return default value divider in 0.001 units
   */
  int32_t getDefaultValueDivider() const;

  /**
   * Returns default value multiplier
   *
   * @return default value multiplier in 0.001 units
   */
  int32_t getDefaultValueMultiplier() const;

  /**
   * Returns default value added
   *
   * @return default value added in 0.001 units
   */
  int64_t getDefaultValueAdded() const;

  /**
   * Returns default precision (number of decimal places)
   *
   * @return default precision in range 0-4
   */
  uint8_t getDefaultValuePrecision() const;

  /**
   * Returns default unit which is displayed before value
   *
   * @param unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] result is stored into this
   */
  void getDefaultUnitBeforeValue(char unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE]);

  /**
   * Returns default unit which is displayed after value
   *
   * @param unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] result is stored into this
   */
  void getDefaultUnitAfterValue(char unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE]);
  ///@}

  /** @name Setters for modification of current channel configuration.
   *
   *  Each call to setters will modify channel config and send it to Supla
   *  Server. This shouldn't be used as a channel config setup step, because
   *  on each device startup it will overwrite user's configuration.
   *
   *  local parameter should be set to true if method is called locally by
   *  device. False is used when called as a result of processing
   *  SetChannelConfig from server. When local is true, channel configuration
   *  will be sent to Supla Server.
   */
  ///@{

  /**
   * Sets refresh interval in milliseconds.
   *
   * @param intervalMs in milliseconds in 0-65535 range
   * @param local
   */
  void setRefreshIntervalMs(int32_t intervalMs, bool local = true);

  /**
   * Sets value divider
   *
   * @param divider in 0.001. 0 is ignored
   * @param local
   */
  void setValueDivider(int32_t divider, bool local = true);

  /**
   * Sets value multiplier
   *
   * @param multiplier in 0.001. 0 is ignored
   * @param local
   */
  void setValueMultiplier(int32_t multiplier, bool local = true);

  /**
   * Sets value added
   *
   * @param added in 0.001 units
   * @param local
   */
  void setValueAdded(int64_t added, bool local = true);

  /**
   * Sets precision
   *
   * @param precision in range 0-4 (number of decimal places)
   * @param local
   */
  void setValuePrecision(uint8_t precision, bool local = true);

  /**
   * Sets unit which is displayed before value
   *
   * @param unit before value - max 15 bytes including terminating null byte
   * @param local
   */
  void setUnitBeforeValue(const char *unit, bool local = true);

  /**
   * Sets unit which is displayed after value
   *
   * @param unit after value - max 15 bytes including terminating null byte
   * @param local
   */
  void setUnitAfterValue(const char *unit, bool local = true);

  /**
   * Sets no space before value. If set to 1, there will be no space added
   * between value and unit before value
   *
   * @param noSpaceBeforeValue
   * @param local
   */
  void setNoSpaceBeforeValue(uint8_t noSpaceBeforeValue, bool local = true);

  /**
   * Sets no space after value. If set to 1, there will be no space added
   * between value and unit after value
   *
   * @param noSpaceAfterValue
   * @param local
   */
  void setNoSpaceAfterValue(uint8_t noSpaceAfterValue, bool local = true);
  // Sets keep history flag

  /**
   * Sets keep history flag. If set to 1, history of values will be stored
   * on server.
   *
   * @param keepHistory
   * @param local
   */
  void setKeepHistory(uint8_t keepHistory, bool local = true);

  // Sets chart type. Allowed values:
  // For Measurement channel:
  // SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_LINEAR
  // SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_BAR
  // SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_CANDLE
  // For Meter channel:
  // SUPLA_GENERAL_PURPOSE_METER_CHART_TYPE_BAR
  // SUPLA_GENERAL_PURPOSE_METER_CHART_TYPE_LINEAR
  /**
   * Sets chart type.
   * For Measurement channel:
   *   SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_LINEAR
   *   SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_BAR
   *   SUPLA_GENERAL_PURPOSE_MEASUREMENT_CHART_TYPE_CANDLE
   * For Meter channel:
   *   SUPLA_GENERAL_PURPOSE_METER_CHART_TYPE_BAR
   *   SUPLA_GENERAL_PURPOSE_METER_CHART_TYPE_LINEAR
   *
   * @param chartType
   * @param local
   */
  void setChartType(uint8_t chartType, bool local = true);
  ///@}

  /** @name Getters for current channel configuration parameters.
   */
  ///@{
  /**
   * Returns refresh interval in milliseconds
   *
   * @return refresh interval in milliseconds
   */
  uint16_t getRefreshIntervalMs() const;

  /**
   * Returns value divider
   *
   * @return value divider in 0.001 units
   */
  int32_t getValueDivider() const;

  /**
   * Returns value multiplier
   *
   * @return value multiplier in 0.001 units
   */
  int32_t getValueMultiplier() const;

  /**
   * Returns value added
   *
   * @return value added in 0.001 units
   */
  int64_t getValueAdded() const;

  /**
   * Returns precision (number of decimal places)
   *
   * @return precision in range 0-4
   */
  uint8_t getValuePrecision() const;

  /**
   * Returns unit which is displayed before value
   *
   * @param unit before value - max 15 bytes including terminating null byte
   */
  void getUnitBeforeValue(char unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE]) const;

  /**
   * Returns unit which is displayed after value
   *
   * @param unit after value - max 15 bytes including terminating null byte
   */
  void getUnitAfterValue(char unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE]) const;

  /**
   * Returns no space before value.
   *
   * @return no space before value - 1 = no space, 0 = space
   */
  uint8_t getNoSpaceBeforeValue() const;

  /**
   * Returns no space after value.
   *
   * @return no space after value - 1 = no space, 0 = space
   */
  uint8_t getNoSpaceAfterValue() const;

  /**
   * Returns keep history flag.
   *
   * @return keep history flag - 1 = keep history, 0 = don't keep history
   */
  uint8_t getKeepHistory() const;

  /**
   * Returns chart type.
   *
   * @return chart type (@see setChartType())
   */
  uint8_t getChartType() const;
  ///@}

 protected:
  void saveConfig();
  virtual void setChannelRefreshIntervalMs(uint16_t intervalMs);

  MeasurementDriver *driver = nullptr;
  uint16_t refreshIntervalMs = 5000;
  uint32_t lastReadTime = 0;
  bool deleteDriver = false;

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
