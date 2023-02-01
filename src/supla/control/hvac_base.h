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

namespace Supla {
namespace Control {

class HvacBase : public ChannelElement {
 public:
  HvacBase();
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

  void scheduleSaveConfig(uint32_t timeMs, bool localChange = false);
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
  _supla_int16_t getTemperatureRoomMin(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureRoomMax(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHeaterCoolerMin(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHeaterCoolerMax(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHisteresisMin(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHisteresisMax(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureAutoOffsetMin(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureAutoOffsetMax(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureRoomMin();
  _supla_int16_t getTemperatureRoomMax();
  _supla_int16_t getTemperatureHeaterCoolerMin();
  _supla_int16_t getTemperatureHeaterCoolerMax();
  _supla_int16_t getTemperatureHisteresisMin();
  _supla_int16_t getTemperatureHisteresisMax();
  _supla_int16_t getTemperatureAutoOffsetMin();
  _supla_int16_t getTemperatureAutoOffsetMax();

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
  bool setTemperatureAutoOffset(_supla_int16_t temperature);
  bool setTemperatureBelowAlarm(_supla_int16_t temperature);
  bool setTemperatureAboveAlarm(_supla_int16_t temperature);
  bool setTemperatureHeaterCoolerMinSetpoint(_supla_int16_t temperature);
  bool setTemperatureHeaterCoolerMaxSetpoint(_supla_int16_t temperature);
  _supla_int16_t getTemperatureFreezeProtection(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHeatProtection(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureEco(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureComfort(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureBoost(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHisteresis(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureAutoOffset(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureBelowAlarm(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureAboveAlarm(THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHeaterCoolerMinSetpoint(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureHeaterCoolerMaxSetpoint(
      THVACTemperatureCfg *temperatures);
  _supla_int16_t getTemperatureFreezeProtection();
  _supla_int16_t getTemperatureHeatProtection();
  _supla_int16_t getTemperatureEco();
  _supla_int16_t getTemperatureComfort();
  _supla_int16_t getTemperatureBoost();
  _supla_int16_t getTemperatureHisteresis();
  _supla_int16_t getTemperatureAutoOffset();
  _supla_int16_t getTemperatureBelowAlarm();
  _supla_int16_t getTemperatureAboveAlarm();
  _supla_int16_t getTemperatureHeaterCoolerMinSetpoint();
  _supla_int16_t getTemperatureHeaterCoolerMaxSetpoint();

  bool isOnOffSupported();
  bool isHeatingSupported();
  bool isCoolingSupported();
  bool isAutoSupported();
  bool isFanSupported();
  bool isDrySupported();

  bool isFunctionSupported(_supla_int_t channelFunction);
  bool isConfigValid(TSD_ChannelConfig_HVAC *config);
  bool isChannelThermometer(uint8_t channelNo);
  bool isAlgorithmValid(unsigned _supla_int16_t algorithm);
  bool areTemperaturesValid(THVACTemperatureCfg *temperatures);

  static bool isTemperatureSetInStruct(THVACTemperatureCfg *temperatures,
                                unsigned _supla_int_t index);
  static _supla_int16_t getTemperatureFromStruct(
      THVACTemperatureCfg *temperatures, unsigned _supla_int_t index);
  static void setTemperatureInStruct(THVACTemperatureCfg *temperatures,
                              unsigned _supla_int_t index,
                              _supla_int16_t temperature);

  bool isTemperatureInRoomConstrain(_supla_int16_t temperature);
  bool isTemperatureInHeaterCoolerConstrain(_supla_int16_t temperature);
  bool isTemperatureFreezeProtectionValid(_supla_int16_t temperature);
  bool isTemperatureHeatProtectionValid(_supla_int16_t temperature);
  bool isTemperatureEcoValid(_supla_int16_t temperature);
  bool isTemperatureComfortValid(_supla_int16_t temperature);
  bool isTemperatureBoostValid(_supla_int16_t temperature);
  bool isTemperatureHisteresisValid(_supla_int16_t temperature);
  bool isTemperatureAutoOffsetValid(_supla_int16_t temperature);
  bool isTemperatureBelowAlarmValid(_supla_int16_t temperature);
  bool isTemperatureAboveAlarmValid(_supla_int16_t temperature);
  // validates temperature against current configuration
  bool isTemperatureHeaterCoolerMinSetpointValid(_supla_int16_t temperature);
  // validates temperature against current configuration
  bool isTemperatureHeaterCoolerMaxSetpointValid(_supla_int16_t temperature);

  bool isTemperatureFreezeProtectionValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureHeatProtectionValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureEcoValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureComfortValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureBoostValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureHisteresisValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureAutoOffsetValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureBelowAlarmValid(THVACTemperatureCfg *temperatures);
  bool isTemperatureAboveAlarmValid(THVACTemperatureCfg *temperatures);
  // validates temperature against configuration send in parameter
  bool isTemperatureHeaterCoolerMinSetpointValid(
      THVACTemperatureCfg *temperatures);
  // validates temperature against configuration send in parameter
  bool isTemperatureHeaterCoolerMaxSetpointValid(
      THVACTemperatureCfg *temperatures);

  void clearChannelConfigChangedFlag();
  void clearWeeklyScheduleChangedFlag();

 private:
  TSD_ChannelConfig_HVAC config = {};
  TSD_ChannelConfig_WeeklySchedule weeklySchedule = {};
  bool isWeeklyScheduleConfigured = false;
  uint8_t channelConfigChangedOffline = 0;
  uint8_t weeklyScheduleChangedOffline = 0;
  bool waitForChannelConfigAndIgnoreIt = false;
  bool waitForWeeklyScheduleAndIgnoreIt = false;
  bool initDone = false;
  uint64_t lastLocalConfigChangeTimestampMs = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_BASE_H_
