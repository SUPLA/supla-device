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

#ifndef SRC_SUPLA_CHANNELS_CHANNEL_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_H_

#include <stdint.h>

#include <supla-common/proto.h>
#include <supla/local_action.h>
#include "channel_types.h"

namespace Supla {

enum class HvacCoolSubfunctionFlag {
  HeatSubfunctionOrNotUsed,
  CoolSubfunction,
};

class Channel : public LocalAction {
 public:
  explicit Channel(int number = -1);
  virtual ~Channel();
  static Channel *Begin();
  static Channel *Last();
  static Channel *GetByChannelNumber(int channelNumber);
  Channel *next();

#ifdef SUPLA_TEST
  static void resetToDefaults();
#endif
  void fillDeviceChannelStruct(TDS_SuplaDeviceChannel_D *deviceChannelStruct);
  void fillDeviceChannelStruct(TDS_SuplaDeviceChannel_E *deviceChannelStruct);

  bool setChannelNumber(int newChannelNumber);

  void setNewValue(double dbl);
  void setNewValue(double temp, double humi);
  void setNewValue(int32_t value);
  void setNewValue(bool value);
  void setNewValue(const TElectricityMeter_ExtendedValue_V3 &emValue);
  void setNewValue(uint8_t red,
                   uint8_t green,
                   uint8_t blue,
                   uint8_t colorBrightness,
                   uint8_t brightness);
  void setNewValue(uint64_t value);
  void setNewValue(const TDSC_RollerShutterValue &value);
  void setNewValue(const TDSC_FacadeBlindValue &value);
  bool setNewValue(const char *newValue);

  void setStateOffline();
  void setStateOnline();
  void setStateOnlineAndNotAvailable();
  void setStateFirmwareUpdateOngoing();
  void setStateOfflineRemoteWakeupNotSupported();
  bool isStateOnline() const;
  bool isStateOnlineAndNotAvailable() const;
  bool isStateOfflineRemoteWakeupNotSupported() const;
  bool isStateFirmwareUpdateOngoing() const;

  // Sets container channel value. fillLevel should contain 0-100 value, any
  // other value will be set to "unknown" value.
  void setContainerFillValue(int8_t fillLevel);
  void setContainerAlarm(bool active);
  void setContainerWarning(bool active);
  void setContainerInvalidSensorState(bool invalid);
  void setContainerSoundAlarmOn(bool soundAlarmOn);

  // Returns 0-100 value for 0-100 %, -1 if not available
  int8_t getContainerFillValue() const;
  bool isContainerAlarmActive() const;
  bool isContainerWarningActive() const;
  bool isContainerInvalidSensorStateActive() const;
  bool isContainerSoundAlarmOn() const;

  /**
   * Sets the open state (open/close) of the Valve channel.
   *
   * @param openState 0-100 range, 0 = closed, 100 = open. For OpenClose variant
   * of the channel, 0 = closed, >= 1 = open (converted to 100)
   */
  void setValveOpenState(uint8_t openState);

  /**
   * Sets the flooding flag of the Valve channel. Flooding flag is set when
   * the valve was closed because of flood/leak detection.
   *
   * @param active
   */
  void setValveFloodingFlag(bool active);

  /**
   * Sets the manually closed flag of the Valve channel. Manually closed flag is
   * set when the valve was closed manually or in other way by valve (i.e.
   * radio).
   *
   * @param active
   */
  void setValveManuallyClosedFlag(bool active);

  /**
   * Sets the motor problem flag of the Valve channel.
   *
   * @param active
   */
  void setValveMotorProblemFlag(bool active);

  /**
   * Returns the open state (open/close) of the Valve channel.
   *
   * @return 0-100 range, 0 = closed, 100 = open
   */
  uint8_t getValveOpenState() const;

  /**
   * Returns the open state (open/close) of the Valve channel.
   *
   * @return true if open
   */
  bool isValveOpen() const;

  /**
   * Returns the flooding flag of the Valve channel.
   *
   * @return true if flooding flag is active
   */
  bool isValveFloodingFlagActive() const;

  /**
   * Returns the manually closed flag of the Valve channel.
   *
   * @return true if manually closed flag is active
   */
  bool isValveManuallyClosedFlagActive() const;

  /**
   * Returns the motor problem flag of the Valve channel.
   *
   * @return true if motor problem flag is active
   */
  bool isValveMotorProblemFlagActive() const;

  double getValueDouble();
  double getValueDoubleFirst();
  double getValueDoubleSecond();
  int32_t getValueInt32();
  uint64_t getValueInt64();
  virtual bool getValueBool();
  uint8_t getValueRed();
  uint8_t getValueGreen();
  uint8_t getValueBlue();
  uint8_t getValueColorBrightness();
  uint8_t getValueBrightness();
  double getLastTemperature();
  uint8_t getValueClosingPercentage() const;
  uint8_t getValueTilt() const;
  bool getValueIsCalibrating() const;

  void setHvacIsOn(bool isOn);
  void setHvacIsOnPercent(uint8_t percent);
  void setHvacMode(uint8_t mode);
  void setHvacSetpointTemperatureHeat(int16_t setpointTemperatureHeat);
  void setHvacSetpointTemperatureCool(int16_t setpointTemperatureCool);
  void clearHvacSetpointTemperatureHeat();
  void clearHvacSetpointTemperatureCool();
  void setHvacFlags(uint16_t alarmsAndFlags);
  void setHvacFlagSetpointTemperatureHeatSet(bool value);
  void setHvacFlagSetpointTemperatureCoolSet(bool value);
  void setHvacFlagHeating(bool value);
  void setHvacFlagCooling(bool value);
  void setHvacFlagWeeklySchedule(bool value);
  void setHvacFlagFanEnabled(bool value);
  void setHvacFlagThermometerError(bool value);
  void setHvacFlagClockError(bool value);
  void setHvacFlagCountdownTimer(bool value);
  void setHvacFlagForcedOffBySensor(bool value);
  void setHvacFlagCoolSubfunction(enum HvacCoolSubfunctionFlag flag);
  void setHvacFlagWeeklyScheduleTemporalOverride(bool value);
  void setHvacFlagBatteryCoverOpen(bool value);
  void setHvacFlagCalibrationError(bool value);
  void setHvacFlagAntifreezeOverheatActive(bool value);
  void clearHvacState();

  uint8_t getHvacIsOnRaw() const;
  bool getHvacIsOnBool() const;
  uint8_t getHvacIsOnPercent() const;

  uint8_t getHvacMode() const;
  // returns mode as a string. If mode parameters is -1 then it returns current
  // channel mode, otherwise mode parameter is used.
  const char *getHvacModeCstr(int mode = -1) const;
  int16_t getHvacSetpointTemperatureHeat() const;
  int16_t getHvacSetpointTemperatureCool() const;
  uint16_t getHvacFlags() const;
  bool isHvacFlagSetpointTemperatureHeatSet() const;
  bool isHvacFlagSetpointTemperatureCoolSet() const;
  bool isHvacFlagHeating() const;
  bool isHvacFlagCooling() const;
  bool isHvacFlagWeeklySchedule() const;
  bool isHvacFlagFanEnabled() const;
  bool isHvacFlagThermometerError() const;
  bool isHvacFlagClockError() const;
  bool isHvacFlagCountdownTimer() const;
  bool isHvacFlagForcedOffBySensor() const;
  enum HvacCoolSubfunctionFlag getHvacFlagCoolSubfunction() const;
  bool isHvacFlagWeeklyScheduleTemporalOverride() const;
  bool isHvacFlagBatteryCoverOpen() const;
  bool isHvacFlagCalibrationError() const;
  bool isHvacFlagAntifreezeOverheatActive() const;

  static bool isHvacFlagSetpointTemperatureHeatSet(const THVACValue *hvacValue);
  static bool isHvacFlagSetpointTemperatureCoolSet(const THVACValue *hvacValue);
  static bool isHvacFlagHeating(const THVACValue *hvacValue);
  static bool isHvacFlagCooling(const THVACValue *hvacValue);
  static bool isHvacFlagWeeklySchedule(const THVACValue *hvacValue);
  static bool isHvacFlagFanEnabled(const THVACValue *hvacValue);
  static bool isHvacFlagThermometerError(const THVACValue *hvacValue);
  static bool isHvacFlagClockError(const THVACValue *hvacValue);
  static bool isHvacFlagCountdownTimer(const THVACValue *hvacValue);
  static bool isHvacFlagForcedOffBySensor(const THVACValue *hvacValue);
  static enum HvacCoolSubfunctionFlag getHvacFlagCoolSubfunction(
      const THVACValue *hvacValue);
  static bool isHvacFlagWeeklyScheduleTemporalOverride(
      const THVACValue *hvacValue);
  static bool isHvacFlagBatteryCoverOpen(const THVACValue *hvacValue);
  static bool isHvacFlagCalibrationError(const THVACValue *hvacValue);
  static bool isHvacFlagAntifreezeOverheatActive(const THVACValue *hvacValue);

  static void setHvacSetpointTemperatureHeat(THVACValue *hvacValue,
                                             int16_t setpointTemperatureHeat);
  static void setHvacSetpointTemperatureCool(THVACValue *hvacValue,
                                            int16_t setpointTemperatureCool);

  THVACValue *getValueHvac();
  const THVACValue *getValueHvac() const;
  static bool isHvacValueValid(const THVACValue *hvacValue);

  virtual bool isExtended() const;
  bool isUpdateReady() const;
  int getChannelNumber() const;
  uint32_t getChannelType() const;

  void setType(uint32_t type);

  // setDefault and setDefaultFunction are the same methods.
  // Second was added for better readability
  void setDefault(uint32_t value);

  /**
   * Set default function.
   * It can be also used in runtime to set current function.
   *
   * @param function
   */
  void setDefaultFunction(uint32_t function);
  uint32_t getDefaultFunction() const;
  bool isFunctionValid(uint32_t function) const;
  void setFlag(uint64_t flag);
  void unsetFlag(uint64_t flag);
  uint64_t getFlags() const;
  void setFuncList(uint32_t functions);
  uint32_t getFuncList() const;
  void addToFuncList(uint32_t function);
  void removeFromFuncList(uint32_t function);
  void setActionTriggerCaps(uint32_t caps);
  uint32_t getActionTriggerCaps();

  void setValidityTimeSec(uint32_t timeSec);
  virtual void sendUpdate();
  virtual TSuplaChannelExtendedValue *getExtValue();
  // Returns true when value was properly converted to EM value.
  // "out" has to be valid pointer to allocated structure.
  virtual bool getExtValueAsElectricityMeter(
      TElectricityMeter_ExtendedValue_V3 *out);
  void setCorrection(double correction, bool forSecondaryValue = false);
  bool isSleepingEnabled();
  bool isWeeklyScheduleAvailable();

  // Returns true if channel is battery powered (for channel state info)
  bool isBatteryPoweredFieldEnabled() const;
  bool isBatteryPowered() const;
  // sets battery powered flag
  void setBatteryPowered(bool);

  /**
   * Returns battery level
   *
   * @return 0..100 (0 = empty, 100 = full), or 255 if not available
   */
  uint8_t getBatteryLevel() const;

  /**
   * Sets battery level
   *
   * @param level 0..100
   */
  void setBatteryLevel(int level);

  // Sets bridge signal strength. Allowed values are 0..100, or 255 to disable
  void setBridgeSignalStrength(unsigned char level);
  uint8_t getBridgeSignalStrength() const;
  bool isBridgeSignalStrengthAvailable() const;

  void setInitialCaption(const char *caption);
  bool isInitialCaptionSet() const;
  const char* getInitialCaption() const;

  /**
   * Sets default icon. Default icon setting is applied by server only when
   * channel has no icon set (i.e. on channel registration). Changing it
   * afterwards will have no effect until device is removed from Cloud.
   *
   * @param iconId 0..255 (depending on channel's function). 0 is used by
   *                default. Other values depends on icon availability in
   *                Cloud. See:
   * https://github.com/SUPLA/supla-cloud/tree/master/web/assets/img/functions
   *                File names follows format: "functionId_iconId-variant.svg",
   *                i.e. 130_2-on.svg - here "2" can be used as iconId for
   *                function 130 (power switch).
   */
  void setDefaultIcon(uint8_t iconId);

  /**
   * Returns default icon
   *
   * @return 0..255
   */
  uint8_t getDefaultIcon() const;

  void fillRawValue(void *value);
  int8_t *getValuePtr();

  void setSubDeviceId(uint8_t subDeviceId);
  uint8_t getSubDeviceId() const;

  bool isRollerShutterRelayType() const;
  void setRelayOvercurrentCutOff(bool value);
  bool isRelayOvercurrentCutOff() const;

  void onRegistered();
  void setSendGetConfig();

  bool isChannelStateEnabled() const;
  void clearSendValue();

 protected:
  void setSendValue();
  bool isValueUpdateReady() const;

  void clearSendGetConfig();
  bool isGetConfigRequested() const;

  void setSendInitialCaption();
  void clearSendInitialCaption();
  bool isInitialCaptionUpdateReady() const;

  void setSendStateInfo();
  void clearSendStateInfo();
  bool isStateInfoUpdateReady() const;

  static Channel *firstPtr;
  Channel *nextPtr = nullptr;

  char *initialCaption = nullptr;

  uint32_t functionsBitmap = 0;

  uint64_t channelFlags = 0;
  uint32_t validityTimeSec = 0;

  int16_t channelNumber = -1;

  uint16_t defaultFunction =
      0;  // function in proto use 32 bit, but there are no functions defined so
          // far that use more than 16 bits

  uint8_t changedFields = 0;  // keeps track of pending updates

  uint8_t batteryLevel = 255;          // 0 - 100%; 255 - not used
  uint8_t batteryPowered = 0;  // 0 - not used, 1 - true, 2 - false
  unsigned char bridgeSignalStrength = 255;  // 0 - 100%; 255 - not used

  // registration parameter
  ChannelType channelType = ChannelType::NOT_SET;

  uint8_t state = 0;
  uint8_t defaultIcon = 0;
  uint8_t subDeviceId = 0;

  union {
    int8_t value[SUPLA_CHANNELVALUE_SIZE] = {};
    TActionTriggerProperties actionTriggerProperties;
    THVACValue hvacValue;
  };
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_CHANNEL_H_
