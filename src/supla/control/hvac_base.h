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
#include <supla/action_handler.h>
#include <time.h>

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

class HvacBase : public ChannelElement, public ActionHandler {
 public:
  explicit HvacBase(Supla::Control::OutputInterface *primaryOutput = nullptr,
                    Supla::Control::OutputInterface *secondaryOutput = nullptr);
  virtual ~HvacBase();

  void onLoadConfig(SuplaDeviceClass *) override;
  void onLoadState() override;
  void onInit() override;
  void onSaveState() override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  void iterateAlways() override;
  bool iterateConnected() override;

  int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *config,
                              bool local = false) override;
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *result,
                               bool local = false) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void handleAction(int event, int action) override;

  // 0 = off, >= 1 enable heating, <= -1 enable cooling
  void setOutput(int value, bool force = false);
  void setTargetMode(int mode, bool keepSchedule = false);
  void clearTemperatureSetpointMin();
  void clearTemperatureSetpointMax();
  void setTemperatureSetpointMin(int tMin);
  void setTemperatureSetpointMax(int tMax);
  int getTemperatureSetpointMin();
  int getTemperatureSetpointMax();
  int getMode();
  int getDefaultManualMode();
  bool isWeeklyScheduleEnabled();
  bool isCountdownEnabled();
  bool isThermostatDisabled();
  bool isManualModeEnabled();

  void saveConfig();
  void saveWeeklySchedule();

  // Below functions are used to set device capabilities.
  void setOnOffSupported(bool supported);
  void setHeatingSupported(bool supported);
  void setCoolingSupported(bool supported);
  void setAutoSupported(bool supported);
  void setFanSupported(bool supported);
  void setDrySupported(bool supported);

  void addAvailableAlgorithm(unsigned _supla_int16_t algorithm);
  // use this function to set value based on local config change
  bool setUsedAlgorithm(unsigned _supla_int16_t newAlgorithm);
  unsigned _supla_int16_t getUsedAlgorithm() const;

  // use this function to set value based on local config change
  bool setMainThermometerChannelNo(uint8_t channelNo);
  uint8_t getMainThermometerChannelNo() const;

  // use this function to set value based on local config change
  bool setAuxThermometerChannelNo(uint8_t channelNo);
  uint8_t getAuxThermometerChannelNo() const;
  // use this function to set value based on local config change
  void setAuxThermometerType(uint8_t type);
  uint8_t getAuxThermometerType() const;

  // use this function to set value based on local config change
  void setAntiFreezeAndHeatProtectionEnabled(bool enebled);
  bool isAntiFreezeAndHeatProtectionEnabled() const;

  // use this function to set value based on local config change
  bool setMinOnTimeS(uint16_t seconds);
  uint16_t getMinOnTimeS() const;
  bool isMinOnOffTimeValid(uint16_t seconds) const;

  // use this function to set value based on local config change
  bool setMinOffTimeS(uint16_t seconds);
  uint16_t getMinOffTimeS() const;

  // use this function to set value based on local config change
  bool setOutputValueOnError(signed char value);
  signed char getOutputValueOnError() const;

  void setDefaultTemperatureRoomMin(int channelFunction,
                                    _supla_int16_t temperature);
  void setDefaultTemperatureRoomMax(int channelFunction,
                                    _supla_int16_t temperature);
  _supla_int16_t getDefaultTemperatureRoomMin() const;
  _supla_int16_t getDefaultTemperatureRoomMax() const;

  void initDefaultConfig();
  void initDefaultWeeklySchedule();
  void initDefaultAlgorithm();

  // Below temperatures defines device capabilities.
  // Configure those values before calling other setTemperature* functions.
  void setTemperatureRoomMin(_supla_int16_t temperature);
  void setTemperatureRoomMax(_supla_int16_t temperature);
  void setTemperatureAuxMin(_supla_int16_t temperature);
  void setTemperatureAuxMax(_supla_int16_t temperature);
  void setTemperatureHisteresisMin(_supla_int16_t temperature);
  void setTemperatureHisteresisMax(_supla_int16_t temperature);
  void setTemperatureAutoOffsetMin(_supla_int16_t temperature);
  void setTemperatureAutoOffsetMax(_supla_int16_t temperature);
  _supla_int16_t getTemperatureRoomMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureRoomMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAuxMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAuxMax(
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
  _supla_int16_t getTemperatureAuxMin() const;
  _supla_int16_t getTemperatureAuxMax() const;
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
  bool setTemperatureAuxMinSetpoint(_supla_int16_t temperature);
  bool setTemperatureAuxMaxSetpoint(_supla_int16_t temperature);
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
  _supla_int16_t getTemperatureAuxMinSetpoint(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureAuxMaxSetpoint(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureFreezeProtection() const;
  _supla_int16_t getTemperatureHeatProtection() const;
  _supla_int16_t getTemperatureEco() const;
  _supla_int16_t getTemperatureComfort() const;
  _supla_int16_t getTemperatureBoost() const;
  _supla_int16_t getTemperatureHisteresis() const;
  _supla_int16_t getTemperatureBelowAlarm() const;
  _supla_int16_t getTemperatureAboveAlarm() const;
  _supla_int16_t getTemperatureAuxMinSetpoint() const;
  _supla_int16_t getTemperatureAuxMaxSetpoint() const;

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
  bool isConfigValid(TChannelConfig_HVAC *config) const;
  bool isWeeklyScheduleValid(
      TChannelConfig_WeeklySchedule *newSchedule) const;
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
  static void clearTemperatureInStruct(THVACTemperatureCfg *temperatures,
                              unsigned _supla_int_t index);

  static int getArrayIndex(int bitIndex);

  bool isTemperatureInRoomConstrain(_supla_int16_t temperature) const;
  bool isTemperatureInAuxConstrain(_supla_int16_t temperature) const;
  bool isTemperatureFreezeProtectionValid(_supla_int16_t temperature) const;
  bool isTemperatureHeatProtectionValid(_supla_int16_t temperature) const;
  bool isTemperatureEcoValid(_supla_int16_t temperature) const;
  bool isTemperatureComfortValid(_supla_int16_t temperature) const;
  bool isTemperatureBoostValid(_supla_int16_t temperature) const;
  bool isTemperatureHisteresisValid(_supla_int16_t temperature) const;
  bool isTemperatureBelowAlarmValid(_supla_int16_t temperature) const;
  bool isTemperatureAboveAlarmValid(_supla_int16_t temperature) const;
  // validates temperature against current configuration
  bool isTemperatureAuxMinSetpointValid(
      _supla_int16_t temperature) const;
  // validates temperature against current configuration
  bool isTemperatureAuxMaxSetpointValid(
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
  bool isTemperatureAuxMinSetpointValid(
      const THVACTemperatureCfg *temperatures) const;
  // validates temperature against configuration send in parameter
  bool isTemperatureAuxMaxSetpointValid(
      const THVACTemperatureCfg *temperatures) const;

  void clearChannelConfigChangedFlag();
  void clearWeeklyScheduleChangedFlag();

  int getWeeklyScheduleProgramId(int index) const;
  int getWeeklyScheduleProgramId(enum DayOfWeek dayOfWeek,
                                  int hour,
                                  int quarter) const;
  int getWeeklyScheduleProgramId(
      const TChannelConfig_WeeklySchedule *schedule, int index) const;
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

  void copyFixedChannelConfigTo(HvacBase *hvac) const;
  void copyFullChannelConfigTo(TChannelConfig_HVAC *hvac) const;
  void turnOn();
  bool turnOnWeeklySchedlue();
  void changeFunction(int newFunction, bool changedLocally);
  _supla_int_t getChannelFunction();

  void addPrimaryOutput(Supla::Control::OutputInterface *output);
  void addSecondaryOutput(Supla::Control::OutputInterface *output);

  void enableDifferentialFunctionSupport();
  bool isDifferentialFunctionSupported() const;
  void enableDomesticHotWaterFunctionSupport();
  bool isDomesticHotWaterFunctionSupported() const;

  bool applyNewRuntimeSettings(int mode,
                               int16_t tMin,
                               int16_t tMax,
                               int32_t durationSec = 0);

  // keeps the temperature setpoints
  bool applyNewRuntimeSettings(int mode,
                               int32_t durationSec = 0);

  _supla_int16_t getLastTemperature();

  static void debugPrintConfigStruct(const TChannelConfig_HVAC *config);

  _supla_int16_t getPrimaryTemp();
  _supla_int16_t getSecondaryTemp();

 private:
  _supla_int16_t getTemperature(int channelNo);
  bool isSensorTempValid(_supla_int16_t temperature) const;
  bool checkOverheatProtection(_supla_int16_t t);
  bool checkAntifreezeProtection(_supla_int16_t t);
  bool checkAuxProtection(_supla_int16_t t);
  bool processWeeklySchedule();
  void setSetpointTemperaturesForCurrentMode(int tMin, int tMax);
  bool checkThermometersStatusForCurrentMode(_supla_int16_t t1,
                                             _supla_int16_t t2) const;
  int evaluateHeatOutputValue(_supla_int16_t tMeasured,
                          _supla_int16_t tTarget);
  int evaluateCoolOutputValue(_supla_int16_t tMeasured,
                          _supla_int16_t tTarget);
  void fixTemperatureSetpoints();
  void clearLastOutputValue();
  void storeLastWorkingMode();
  void applyConfigWithoutValidation(TChannelConfig_HVAC *hvacConfig);
  int channelFunctionToIndex(int channelFunction) const;

  TChannelConfig_HVAC config = {};
  TChannelConfig_WeeklySchedule weeklySchedule = {};
  bool isWeeklyScheduleConfigured = false;
  uint8_t channelConfigChangedOffline = 0;
  uint8_t weeklyScheduleChangedOffline = 0;
  bool firstChannelConfigAfterRegister = false;
  bool waitForChannelConfigAndIgnoreIt = false;
  bool waitForWeeklyScheduleAndIgnoreIt = false;
  bool initDone = false;
  // primaryOutput can be used for heating or cooling (cooling is supported
  // when secondaryOutput is not used, in such case "AUTO" mode is not
  // available)
  Supla::Control::OutputInterface *primaryOutput = nullptr;
  // secondaryOutput can be used only for cooling
  Supla::Control::OutputInterface *secondaryOutput = nullptr;
  THVACValue lastWorkingMode = {};
  time_t countdownTimerEnds = 0;
  uint32_t lastConfigChangeTimestampMs = 0;
  uint32_t lastIterateTimestampMs = 0;
  uint32_t lastOutputStateChangeTimestampMs = 0;
  int lastValue = -1000;  // set out of output value range
  _supla_int16_t lastTemperature = 0;

  _supla_int16_t defaultTemperatureRoomMin[6] = {
      1000,  // default min temperature for all other functions or when value is
             // set to INT16_MIN
      1000,  // HEAT
      1000,  // COOL
      1000,  // AUTO
      -5000,  // DIFFERENTIAL
      1000,   // DOMESTIC_HOT_WATER
  };
  _supla_int16_t defaultTemperatureRoomMax[6] = {
      4000,  // default min temperature for all other functions or when value is
             // set to INT16_MIN
      4000,  // HEAT
      4000,  // COOL
      4000,  // AUTO
      5000,  // DIFFERENTIAL
      9000,   // DOMESTIC_HOT_WATER
  };
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_BASE_H_
