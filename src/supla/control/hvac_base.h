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

#define HVAC_BASE_FLAG_IGNORE_DEFAULT_PUMP (1 << 0)
#define HVAC_BASE_FLAG_IGNORE_DEFAULT_HEAT_OR_COLD (1 << 1)

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

enum class LocalUILock : uint8_t {
  None = 0,
  Full = 1,
  Temperature = 2,
  NotSupported = 255
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
  void purgeConfig() override;

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *config,
                              bool local = false) override;
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *newWeeklySchedule,
                               bool altSchedule = false,
                               bool local = false) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void handleChannelConfigFinished() override;

  void handleAction(int event, int action) override;

  void setTargetMode(int mode, bool keepSchedule = false);
  void clearTemperatureSetpointHeat();
  void clearTemperatureSetpointCool();
  void setTemperatureSetpointHeat(int tHeat);
  void setTemperatureSetpointCool(int tCool);
  int getTemperatureSetpointHeat();
  int getTemperatureSetpointCool();
  int getMode();
  int getDefaultManualMode();
  bool isWeeklyScheduleEnabled();
  bool isCountdownEnabled();
  bool isThermostatDisabled();
  bool isManualModeEnabled();

  void saveConfig();
  void saveWeeklySchedule();

  // Below functions are used to set device capabilities.
  void setHeatingAndCoolingSupported(bool supported);
  void setHeatCoolSupported(bool supported);
  void setFanSupported(bool supported);
  void setDrySupported(bool supported);

  void addAvailableAlgorithm(unsigned _supla_int16_t algorithm);
  void removeAvailableAlgorithm(unsigned _supla_int16_t algorithm);
  bool isOutputControlledInternally() const;
  // use this function to set value based on local config change
  bool setUsedAlgorithm(unsigned _supla_int16_t newAlgorithm);
  unsigned _supla_int16_t getUsedAlgorithm(bool forAux = false) const;
  void setButtonTemperatureStep(int16_t step);
  int16_t getCurrentHysteresis(bool forAux) const;

  // Local UI blocking configuration.
  // Those options doesn't have any functional effect on HvacBase behavior.
  // HvacBase only provides interface for setting/getting these options.
  // If you plan to use them, please implement proper local UI logic in
  // your application.
  void addLocalUILockCapability(enum LocalUILock capability);
  void removeLocalUILockCapability(enum LocalUILock capability);

  enum LocalUILock getLocalUILockCapabilityAsEnum(uint8_t capability) const;
  void setLocalUILockCapabilities(uint8_t capabilities);
  uint8_t getLocalUILockCapabilities() const;
  bool isLocalUILockCapabilitySupported(enum LocalUILock capability) const;

  bool setLocalUILock(enum LocalUILock lock);
  enum LocalUILock getLocalUILock() const;

  void setLocalUILockTemperatureMin(int16_t min);
  // Returns min allowed temperature setpoint via local UI.
  // Returns INT16_MIN if configured value is not in room min/max range
  int16_t getLocalUILockTemperatureMin() const;

  void setLocalUILockTemperatureMax(int16_t max);
  // Returns max allowed temperature setpoint via local UI.
  // Returns INT16_MIN if configured value is not in room min/max range
  int16_t getLocalUILockTemperatureMax() const;

  // Subfunction can be set only for HVAC_THERMOSTAT channel function
  // SUPLA_HVAC_SUBFUNCTION_*
  void setSubfunction(uint8_t subfunction);
  void setDefaultSubfunction(uint8_t subfunction);

  // use this function to set value based on local config change
  bool setMainThermometerChannelNo(int16_t channelNo);
  int16_t getMainThermometerChannelNo() const;

  // use this function to set value based on local config change
  bool setAuxThermometerChannelNo(int16_t channelNo);
  int16_t getAuxThermometerChannelNo() const;
  // use this function to set value based on local config change
  void setAuxThermometerType(uint8_t type);
  uint8_t getAuxThermometerType() const;

  bool setPumpSwitchChannelNo(uint8_t channelNo);
  void clearPumpSwitchChannelNo();
  int16_t getPumpSwitchChannelNo() const;
  bool isPumpSwitchSet() const;
  bool setHeatOrColdSourceSwitchChannelNo(uint8_t channelNo);
  void clearHeatOrColdSourceSwitchChannelNo();
  int16_t getHeatOrColdSourceSwitchChannelNo() const;
  bool isHeatOrColdSourceSwitchSet() const;
  bool setMasterThermostatChannelNo(uint8_t channelNo);
  void clearMasterThermostatChannelNo();
  int16_t getMasterThermostatChannelNo() const;
  bool isMasterThermostatSet() const;

  // use this function to set value based on local config change
  void setAntiFreezeAndHeatProtectionEnabled(bool enebled);
  bool isAntiFreezeAndHeatProtectionEnabled() const;
  void setAuxMinMaxSetpointEnabled(bool enabled);
  bool isAuxMinMaxSetpointEnabled() const;

  void setTemperatureSetpointChangeSwitchesToManualMode(bool enabled);
  bool isTemperatureSetpointChangeSwitchesToManualMode() const;

  // only for HEAT_COOL thremostats:
  void setUseSeparateHeatCoolOutputs(bool enabled);
  bool isUseSeparateHeatCoolOutputs() const;

  void setTemperatureControlType(uint8_t);

  uint8_t getTemperatureControlType() const;
  bool isTemperatureControlTypeMain() const;
  bool isTemperatureControlTypeAux() const;

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

  void setDefaultTemperatureRoomMin(int32_t channelFunction,
                                    _supla_int16_t temperature);
  void setDefaultTemperatureRoomMax(int32_t channelFunction,
                                    _supla_int16_t temperature);
  _supla_int16_t getDefaultTemperatureRoomMin() const;
  _supla_int16_t getDefaultTemperatureRoomMax() const;

  void initDefaultConfig();
  void initDefaultWeeklySchedule();
  void initDefaultAlgorithm();
  void suspendIterateAlways();

  // Below temperatures defines device capabilities.
  // Configure those values before calling other setTemperature* functions.
  void setTemperatureRoomMin(_supla_int16_t temperature);
  void setTemperatureRoomMax(_supla_int16_t temperature);
  void setTemperatureAuxMin(_supla_int16_t temperature);
  void setTemperatureAuxMax(_supla_int16_t temperature);
  void setTemperatureHisteresisMin(_supla_int16_t temperature);
  void setTemperatureHisteresisMax(_supla_int16_t temperature);
  void setTemperatureHeatCoolOffsetMin(_supla_int16_t temperature);
  void setTemperatureHeatCoolOffsetMax(_supla_int16_t temperature);
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
  _supla_int16_t getTemperatureHeatCoolOffsetMin(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureHeatCoolOffsetMax(
      const THVACTemperatureCfg *temperatures) const;
  _supla_int16_t getTemperatureRoomMin() const;
  _supla_int16_t getTemperatureRoomMax() const;
  _supla_int16_t getTemperatureMainMin() const;
  _supla_int16_t getTemperatureMainMax() const;
  _supla_int16_t getTemperatureAuxMin() const;
  _supla_int16_t getTemperatureAuxMax() const;
  _supla_int16_t getTemperatureHisteresisMin() const;
  _supla_int16_t getTemperatureHisteresisMax() const;
  _supla_int16_t getTemperatureHeatCoolOffsetMin() const;
  _supla_int16_t getTemperatureHeatCoolOffsetMax() const;

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
  bool setTemperatureAuxHisteresis(_supla_int16_t temperature);
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
  _supla_int16_t getTemperatureAuxHisteresis(
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
  _supla_int16_t getTemperatureAuxHisteresis() const;
  _supla_int16_t getTemperatureBelowAlarm() const;
  _supla_int16_t getTemperatureAboveAlarm() const;
  _supla_int16_t getTemperatureAuxMinSetpoint() const;
  _supla_int16_t getTemperatureAuxMaxSetpoint() const;

  // Below methods check if specific function is supported by thermostat.
  // Even if function is supported, it doesn't mean that new mode setting will
  // be valid, becuase this depends on configured Function.
  bool isHeatingAndCoolingSupported() const;
  bool isHeatCoolSupported() const;
  bool isFanSupported() const;
  bool isDrySupported() const;

  bool isHeatingSubfunction() const;
  bool isCoolingSubfunction() const;

  bool isFunctionSupported(_supla_int_t channelFunction) const;
  bool isConfigValid(TChannelConfig_HVAC *config) const;
  bool isWeeklyScheduleValid(
      TChannelConfig_WeeklySchedule *newSchedule,
      bool isAltWeeklySchedule = false) const;
  bool isChannelThermometer(int16_t channelNo) const;
  bool isChannelBinarySensor(int16_t channelNo) const;
  bool isAlgorithmValid(unsigned _supla_int16_t algorithm) const;
  bool areTemperaturesValid(const THVACTemperatureCfg *temperatures) const;
  bool fixTempearturesConfig();

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

  static int32_t getArrayIndex(int32_t bitIndex);

  bool isTemperatureInMainConstrain(_supla_int16_t temperature) const;
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
  bool isTemperatureInHeatCoolConstrain(_supla_int16_t tHeat,
                                    _supla_int16_t tCool) const;

  bool isTemperatureFreezeProtectionValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureHeatProtectionValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureEcoValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureComfortValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureBoostValid(const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureHisteresisValid(
      const THVACTemperatureCfg *temperatures) const;
  bool isTemperatureAuxHisteresisValid(
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

  int getWeeklyScheduleProgramId(
      const TChannelConfig_WeeklySchedule *schedule, int index) const;
  bool isProgramValid(const TWeeklyScheduleProgram &program,
                      bool isAltWeeklySchedule) const;

  bool setProgram(int programId,
                  unsigned char mode,
                  _supla_int16_t tHeat,
                  _supla_int16_t tCool,
                  bool isAltWeeklySchedule = false);
  // index is the number of quarter in week
  bool setWeeklySchedule(int index,
                         int programId,
                         bool isAltWeeklySchedule = false);
  // schedule internally works on 15 min intervals (quarters)
  bool setWeeklySchedule(enum DayOfWeek dayOfWeek,
                         int hour,
                         int quarter,
                         int programId,
                         bool isAltWeeklySchedule = false);
  int calculateIndex(enum DayOfWeek dayOfWeek,
                     int hour,
                     int quarter) const;
  int getCurrentQuarter() const;
  TWeeklyScheduleProgram getCurrentProgram() const;
  int getCurrentProgramId() const;
  TWeeklyScheduleProgram getProgramAt(int quarterIndex) const;
  TWeeklyScheduleProgram getProgramById(int programId,
                                        bool isAltWeeklySchedule = false) const;

  void copyFixedChannelConfigTo(HvacBase *hvac) const;
  void copyFullChannelConfigTo(TChannelConfig_HVAC *hvac) const;
  void turnOn();
  bool turnOnWeeklySchedlue();
  void changeFunction(uint32_t newFunction, bool changedLocally);
  _supla_int_t getChannelFunction();

  void addPrimaryOutput(Supla::Control::OutputInterface *output);
  void addSecondaryOutput(Supla::Control::OutputInterface *output);

  void enableDifferentialFunctionSupport();
  bool isDifferentialFunctionSupported() const;
  void enableDomesticHotWaterFunctionSupport();
  bool isDomesticHotWaterFunctionSupported() const;

  bool applyNewRuntimeSettings(int mode,
                               int16_t tHeat,
                               int16_t tCool,
                               int32_t durationSec = 0);

  // keeps the temperature setpoints
  bool applyNewRuntimeSettings(int mode,
                               int32_t durationSec = 0);

  _supla_int16_t getLastTemperature();

  bool setBinarySensorChannelNo(int16_t channelNo);
  int16_t getBinarySensorChannelNo() const;

  static void debugPrintConfigStruct(const TChannelConfig_HVAC *config, int id);
  static void debugPrintConfigDiff(const TChannelConfig_HVAC *configCurrent,
                                   const TChannelConfig_HVAC *configNew,
                                   int id);
  static const char* temperatureName(int32_t index);
  static void debugPrintProgram(const TWeeklyScheduleProgram *program, int id);

  _supla_int16_t getPrimaryTemp();
  _supla_int16_t getSecondaryTemp();

  bool isWeelkySchedulManualOverrideMode() const;
  void clearWaitingFlags();

  void allowWrapAroundTemperatureSetpoints();

  void enableInitialConfig();

  // returns Linux timestamp in seconds when current countdown timer will end.
  // It return 1 if countdown timer is not set
  time_t getCountDownTimerEnds() const;
  int32_t getRemainingCountDownTimeSec() const;
  void stopCountDownTimer();

  bool ignoreAggregatorForRelay(int32_t relayChannelNumber) const;
  void setIgnoreDefaultPumpForAggregator(bool);
  void setIgnoreDefaultHeatOrColdSourceForAggregator(bool);

  bool isAltWeeklySchedulePossible() const;

  /**
   * Returns true if thermostat output is disabled by binary sensor state
   * (i.e. by open window).
   *
   * @return true if forced off
   */
  bool isHvacFlagForcedOffBySensor() const;

  HvacParameterFlags parameterFlags = {};

 protected:
  // 0 = off, >= 1 enable heating, <= -1 enable cooling
  void setOutput(int value, bool force = false);
  void updateChannelState();
  // Implement this method to apply additional validations and corrections
  // to HVAC configuration. Return true when correction was done and it will
  // be shared with server.
  virtual bool applyAdditionalValidation(TChannelConfig_HVAC *hvacConfig);
  void clearLastOutputValue();

 private:
  _supla_int16_t getTemperature(int channelNo);
  // returns true if forced off should be set
  bool getForcedOffSensorState();
  bool isSensorTempValid(_supla_int16_t temperature) const;
  bool checkOverheatProtection(_supla_int16_t t);
  bool checkAntifreezeProtection(_supla_int16_t t);
  bool checkAuxProtection(_supla_int16_t t);
  bool isAuxProtectionEnabled() const;
  bool processWeeklySchedule();
  void setSetpointTemperaturesForCurrentMode(int16_t tHeat, int16_t tCool);
  bool checkThermometersStatusForCurrentMode(_supla_int16_t t1,
                                             _supla_int16_t t2) const;
  int evaluateHeatOutputValue(_supla_int16_t tMeasured,
                          _supla_int16_t tTarget, bool forAux = false);
  int evaluateCoolOutputValue(_supla_int16_t tMeasured,
                          _supla_int16_t tTarget, bool forAux = false);
  void fixTemperatureSetpoints();
  void storeLastWorkingMode();
  void applyConfigWithoutValidation(TChannelConfig_HVAC *hvacConfig);
  int32_t channelFunctionToIndex(int32_t channelFunction) const;
  void changeTemperatureSetpointsBy(int16_t tHeat, int16_t tCool);
  void updateTimerValue();
  bool fixReadonlyParameters(TChannelConfig_HVAC *hvacConfig);
  bool fixReadonlyTemperature(int32_t temperatureIndex,
                              THVACTemperatureCfg *newTemp);

  bool registerInAggregator(int16_t channelNo);
  void unregisterInAggregator(int16_t channelNo);

  int16_t getClosestValidTemperature(int16_t temperature) const;

  TChannelConfig_HVAC config = {};
  TChannelConfig_HVAC *initialConfig = nullptr;
  // primaryOutput can be used for heating or cooling (cooling is supported
  // when secondaryOutput is not used, in such case "AUTO" mode is not
  // available)
  Supla::Control::OutputInterface *primaryOutput = nullptr;
  // secondaryOutput can be used only for cooling
  Supla::Control::OutputInterface *secondaryOutput = nullptr;

  TChannelConfig_WeeklySchedule weeklySchedule = {};
  TChannelConfig_WeeklySchedule altWeeklySchedule = {};

  THVACValue lastWorkingMode = {};

  bool isWeeklyScheduleConfigured = false;
  bool configFinishedReceived = true;
  bool defaultConfigReceived = false;
  bool weeklyScheduleReceived = false;
  bool altWeeklyScheduleReceived = false;
  bool initDone = false;
  bool serverChannelFunctionValid = true;
  bool wrapAroundTemperatureSetpoints = false;
  bool registeredInRelayHvacAggregator = false;
  bool startupDelay = true;
  bool forcedByAux = false;

  uint8_t channelConfigChangedOffline = 0;
  uint8_t weeklyScheduleChangedOffline = 0;
  uint8_t lastManualMode = 0;
  uint8_t previousSubfunction = 0;
  uint8_t defaultSubfunction = 0;
  int16_t defaultMainThermometer = -1;
  int16_t defaultAuxThermometer = -1;
  int16_t defaultBinarySensor = -1;
  int16_t defaultPumpSwitch = -1;
  int16_t defaultHeatOrColdSourceSwitch = -1;
  int16_t defaultMasterThermostat = -1;
  int16_t defaultChannelsFlags = 0;

  int8_t lastProgramManualOverride = -1;
  int8_t lastValue = -111;  // set out of output value range
  uint8_t configFixAttempt = 0;

  int16_t lastManualSetpointHeat = INT16_MIN;
  int16_t lastManualSetpointCool = INT16_MIN;
  int16_t lastTemperature = 0;
  int16_t buttonTemperatureStep = 50;  // 0.5 degrees

  uint32_t lastConfigChangeTimestampMs = 0;
  uint32_t lastIterateTimestampMs = 0;
  uint32_t lastOutputStateChangeTimestampMs = 0;
  uint32_t timerUpdateTimestamp = 0;

  time_t countdownTimerEnds = 1;

  int16_t defaultTemperatureRoomMin[6] = {
      500,  // default min temperature for all other functions or when value is
             // set to INT16_MIN
      500,  // HVAC_THERMOSTAT (heat or cool)
      500,  // AUTO
      -5000,  // DIFFERENTIAL
      1000,   // DOMESTIC_HOT_WATER
  };
  int16_t defaultTemperatureRoomMax[6] = {
      4000,  // default max temperature for all other functions or when value is
             // set to INT16_MIN
      4000,  // HVAC_THERMOSTAT (heat or cool)
      4000,  // AUTO
      5000,  // DIFFERENTIAL
      7500,   // DOMESTIC_HOT_WATER
  };
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_HVAC_BASE_H_
