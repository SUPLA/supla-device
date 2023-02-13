/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_CONTROL_HVAC_BASE_H_
#define SRC_SUPLA_CONTROL_HVAC_BASE_H_

#include <supla/channel_element.h>
#include "supla/sensor/thermometer.h"

namespace Supla {

enum DayOfWeek {
  DayOfWeek_Sunday = 0,
  DayOfWeek_Monday = 1,
  DayOfWeek_Tuesday = 2,
  DayOfWeek_Wednesday = 3,
  DayOfWeek_Thursday = 4,
  DayOfWeek_Friday = 5,
  DayOfWeek_Saturday = 6
};

namespace Control {

class OutputInterface;

class HvacBase : public ChannelElement {
 public:
  explicit HvacBase(Supla::Control::OutputInterface *output = nullptr);
  virtual ~HvacBase();

  void onLoadConfig() override;
  void onLoadState() override;
  void onInit() override;
  void onSaveState() override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  void iterateAlways() override;
  bool iterateConnected() override;

  uint8_t handleChannelConfig(TSD_ChannelConfig *config) override;
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *result) override;
  void handleSetChannelConfigResult(
      TSD_SetChannelConfigResult *result) override;

  void setPrimaryThermometer(Supla::Sensor::Thermometer *t);
  void setSecondaryThermometer(Supla::Sensor::Thermometer *t);
  _supla_int16_t getPrimaryTemp();
  _supla_int16_t getSecondaryTemp();

  void setOutput(int value);

  void saveConfig();
  void saveWeeklySchedule();

  // Below functions are used to set device capabilities.
  void setOnOffSupported(bool supported);
  void setHeatingSupported(bool supported);
  void setCoolingSupported(bool supported);
  void setAutoSupported(bool supported);
  void setFanSupported(bool supported);
  void setDrySupported(bool supported);

  void addAlgorithmCap(unsigned _supla_int16_t algorithm);
  // use this function to set value based on local input change
  bool setUsedAlgorithm(unsigned _supla_int16_t newAlgorithm);
  unsigned _supla_int16_t getUsedAlgorithm() const;

  // use this function to set value based on local input change
  bool setMainThermometerChannelNo(uint8_t channelNo);
  uint8_t getMainThermometerChannelNo() const;

  // use this function to set value based on local input change
  bool setHeaterCoolerThermometerChannelNo(uint8_t channelNo);
  uint8_t getHeaterCoolerThermometerChannelNo() const;
  // use this function to set value based on local input change
  void setHeaterCoolerThermometerType(uint8_t type);
  uint8_t getHeaterCoolerThermometerType() const;

  // use this function to set value based on local input change
  void setAntiFreezeAndHeatProtectionEnabled(bool enebled);
  bool isAntiFreezeAndHeatProtectionEnabled() const;

  // use this function to set value based on local input change
  bool setMinOnTimeS(uint16_t seconds);
  uint16_t getMinOnTimeS() const;
  bool isMinOnOffTimeValid(uint16_t seconds) const;

  // use this function to set value based on local input change
  bool setMinOffTimeS(uint16_t seconds);
  uint16_t getMinOffTimeS() const;

  // Below temperatures defines device capabilities.
  // Configure those values before calling other setTemperature* functions.
  void setTemperatureRoomMin(_supla_int16_t temperature);
  void setTemperatureRoomMax(_supla_int16_t temperature);
  void setTemperatureHeaterCoolerMin(_supla_int16_t temperature);
  void setTemperatureHeaterCoolerMax(_supla_int16_t temperature);
  void setTemperatureHisteresisMin(_supla_int16_t temperature);
  void setTemperatureHisteresisMax(_supla_int16_t temperature);
  void setTemperatureAutoOffsetMin(_supla_int16_t temperature);
  void setTemperatureAutoOffsetMax(_supla_int16_t temperature);
  _supla_int16_t getTemperatureRoomMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureRoomMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeaterCoolerMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeaterCoolerMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHisteresisMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHisteresisMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAutoOffsetMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAutoOffsetMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureRoomMin() const;
  _supla_int16_t getTemperatureRoomMax() const;
  _supla_int16_t getTemperatureHeaterCoolerMin() const;
  _supla_int16_t getTemperatureHeaterCoolerMax() const;
  _supla_int16_t getTemperatureHisteresisMin() const;
  _supla_int16_t getTemperatureHisteresisMax() const;
  _supla_int16_t getTemperatureAutoOffsetMin() const;
  _supla_int16_t getTemperatureAutoOffsetMax() const;

  // Below functions are used to set device configuration - those can
  // be modified by user within limits defined by set* functions above.
  // Set may fail if value is out of range defined by set* functions above.
  // use those set functions to set value based on local input change
  bool setTemperatureFreezeProtection(_supla_int16_t temperature);
  bool setTemperatureHeatProtection(_supla_int16_t temperature);
  bool setTemperatureEco(_supla_int16_t temperature);
  bool setTemperatureComfort(_supla_int16_t temperature);
  bool setTemperatureBoost(_supla_int16_t temperature);
  bool setTemperatureHisteresis(_supla_int16_t temperature);
  bool setTemperatureBelowAlarm(_supla_int16_t temperature);
  bool setTemperatureAboveAlarm(_supla_int16_t temperature);
  bool setTemperatureHeaterCoolerMinSetpoint(_supla_int16_t temperature);
  bool setTemperatureHeaterCoolerMaxSetpoint(_supla_int16_t temperature);
  _supla_int16_t getTemperatureFreezeProtection(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeatProtection(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureEco(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureComfort(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureBoost(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHisteresis(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureBelowAlarm(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAboveAlarm(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeaterCoolerMinSetpoint(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeaterCoolerMaxSetpoint(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureFreezeProtection() const;
  _supla_int16_t getTemperatureHeatProtection() const;
  _supla_int16_t getTemperatureEco() const;
  _supla_int16_t getTemperatureComfort() const;
  _supla_int16_t getTemperatureBoost() const;
  _supla_int16_t getTemperatureHisteresis() const;
  _supla_int16_t getTemperatureBelowAlarm() const;
  _supla_int16_t getTemperatureAboveAlarm() const;
  _supla_int16_t getTemperatureHeaterCoolerMinSetpoint() const;
  _supla_int16_t getTemperatureHeaterCoolerMaxSetpoint() const;

  // Below methods check if specific function is supported by thermostat.
  // Even if function is supported, it doesn't mean that new mode setting will
  // be valid, becuase this depends on configured Function.
  bool isOnOffSupported() const;
  bool isHeatingSupported() const;
  bool isCoolingSupported() const;
  bool isAutoSupported() const;
  bool isFanSupported() const;
  bool isDrySupported() const;

  bool isFunctionSupported(_supla_int_t channelFunction) const;
  bool isConfigValid(TSD_ChannelConfig_HVAC *config) const;
  bool isWeeklyScheduleValid(
      TSD_ChannelConfig_WeeklySchedule *newSchedule) const;
  bool isChannelThermometer(uint8_t channelNo) const;
  bool isAlgorithmValid(unsigned _supla_int16_t algorithm) const;
  bool areTemperaturesValid(const THVACTemperatureCfg *temperatures) const;

  // Check if mode is supported by currently configured Function
  bool isModeSupported(int mode) const;

  static bool isTemperatureSetInStruct(const THVACTemperatureCfg *temperatures,
                                unsigned _supla_int_t index);
  static _supla_int16_t getTemperatureFromStruct(
      const THVACTemperatureCfg *temperatures, unsigned _supla_int_t index);
  static void setTemperatureInStruct(THVACTemperatureCfg *temperatures,
                              unsigned _supla_int_t index,
                              _supla_int16_t temperature);

  bool isTemperatureInRoomConstrain(_supla_int16_t temperature) const;
  bool isTemperatureInHeaterCoolerConstrain(_supla_int16_t temperature) const;
  bool isTemperatureFreezeProtectionValid(_supla_int16_t temperature) const;
  bool isTemperatureHeatProtectionValid(_supla_int16_t temperature) const;
  bool isTemperatureEcoValid(_supla_int16_t temperature) const;
  bool isTemperatureComfortValid(_supla_int16_t temperature) const;
  bool isTemperatureBoostValid(_supla_int16_t temperature) const;
  bool isTemperatureHisteresisValid(_supla_int16_t temperature) const;
  bool isTemperatureBelowAlarmValid(_supla_int16_t temperature) const;
  bool isTemperatureAboveAlarmValid(_supla_int16_t temperature) const;
  // validates temperature against current configuration
  bool isTemperatureHeaterCoolerMinSetpointValid(
      _supla_int16_t temperature) const;
  // validates temperature against current configuration
  bool isTemperatureHeaterCoolerMaxSetpointValid(
      _supla_int16_t temperature) const;
  bool isTemperatureInAutoConstrain(_supla_int16_t tMin,
                                    _supla_int16_t tMax) const;

  bool isTemperatureFreezeProtectionValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureHeatProtectionValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureEcoValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureComfortValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureBoostValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureHisteresisValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureBelowAlarmValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureAboveAlarmValid(
      const THVACTemperatureCfg *temperatures) const;
  // validates temperature against configuration send in parameter
  bool isTemperatureHeaterCoolerMinSetpointValid(
      const THVACTemperatureCfg *temperatures) const;
  // validates temperature against configuration send in parameter
  bool isTemperatureHeaterCoolerMaxSetpointValid(
      const THVACTemperatureCfg *temperatures) const;

  void clearChannelConfigChangedFlag();
  void clearWeeklyScheduleChangedFlag();

  int getWeeklyScheduleProgramId(int index) const;
  int getWeeklyScheduleProgramId(enum DayOfWeek dayOfWeek,
                                  int hour,
                                  int quarter) const;
  int getWeeklyScheduleProgramId(
      const TSD_ChannelConfig_WeeklySchedule *schedule, int index) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program) const;

  bool setProgram(int programId,
                  unsigned char mode,
                  _supla_int16_t tMin,
                  _supla_int16_t tMax);
  // index is the number of quarter in week
  bool setWeeklySchedule(int index, int programId);
  // schedule internally works on 15 min intervals (quarters)
  bool setWeeklySchedule(enum DayOfWeek dayOfWeek,
                         int hour,
                         int quarter,
                         int programId);
  int calculateIndex(enum DayOfWeek dayOfWeek,
                     int hour,
                     int quarter) const;
  TWeeklyScheduleProgram getProgram(int programId) const;

  void copyFixedChannelConfigTo(HvacBase *hvac);

 private:
  _supla_int16_t getTemperature(Supla::Sensor::Thermometer *t);
  bool checkOverheatProtection();
  bool checkAntifreezeProtection();

  TSD_ChannelConfig_HVAC config = {};
  TSD_ChannelConfig_WeeklySchedule weeklySchedule = {};
  bool isWeeklyScheduleConfigured = false;
  uint8_t channelConfigChangedOffline = 0;
  uint8_t weeklyScheduleChangedOffline = 0;
  bool waitForChannelConfigAndIgnoreIt = false;
  bool waitForWeeklyScheduleAndIgnoreIt = false;
  bool initDone = false;
  Supla::Control::OutputInterface *output = nullptr;
  Supla::Sensor::Thermometer *primaryThermometer = nullptr;
  Supla::Sensor::Thermometer *secondaryThermometer = nullptr;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_BASE_H_
