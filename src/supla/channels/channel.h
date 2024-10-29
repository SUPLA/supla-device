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
  void setNewValue(const TElectricityMeter_ExtendedValue_V2 &emValue);
  void setNewValue(uint8_t red,
                   uint8_t green,
                   uint8_t blue,
                   uint8_t colorBrightness,
                   uint8_t brightness);
  void setNewValue(uint64_t value);
  void setNewValue(const TDSC_RollerShutterValue &value);
  bool setNewValue(const char *newValue);

  void setOffline();
  void setOnline();
  void setOnlineAndNotAvailable();
  bool isOnline() const;
  bool isOnlineAndNotAvailable() const;

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

  void setHvacIsOn(int8_t isOn);
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
  void clearHvacState();

  uint8_t getHvacIsOn();
  uint8_t getHvacMode() const;
  // returns mode as a string. If mode parameters is -1 then it returns current
  // channel mode, otherwise mode parameter is used.
  const char *getHvacModeCstr(int mode = -1) const;
  int16_t getHvacSetpointTemperatureHeat();
  int16_t getHvacSetpointTemperatureCool();
  uint16_t getHvacFlags();
  bool isHvacFlagSetpointTemperatureHeatSet();
  bool isHvacFlagSetpointTemperatureCoolSet();
  bool isHvacFlagHeating();
  bool isHvacFlagCooling();
  bool isHvacFlagWeeklySchedule();
  bool isHvacFlagFanEnabled();
  bool isHvacFlagThermometerError();
  bool isHvacFlagClockError();
  bool isHvacFlagCountdownTimer();
  bool isHvacFlagForcedOffBySensor();
  enum HvacCoolSubfunctionFlag getHvacFlagCoolSubfunction();
  bool isHvacFlagWeeklyScheduleTemporalOverride();
  bool isHvacFlagBatteryCoverOpen();

  static bool isHvacFlagSetpointTemperatureHeatSet(THVACValue *hvacValue);
  static bool isHvacFlagSetpointTemperatureCoolSet(THVACValue *hvacValue);
  static bool isHvacFlagHeating(THVACValue *hvacValue);
  static bool isHvacFlagCooling(THVACValue *hvacValue);
  static bool isHvacFlagWeeklySchedule(THVACValue *hvacValue);
  static bool isHvacFlagFanEnabled(THVACValue *hvacValue);
  static bool isHvacFlagThermometerError(THVACValue *hvacValue);
  static bool isHvacFlagClockError(THVACValue *hvacValue);
  static bool isHvacFlagCountdownTimer(THVACValue *hvacValue);
  static bool isHvacFlagForcedOffBySensor(THVACValue *hvacValue);
  static enum HvacCoolSubfunctionFlag getHvacFlagCoolSubfunction(
      THVACValue *hvacValue);
  static bool isHvacFlagWeeklyScheduleTemporalOverride(THVACValue *hvacValue);
  static bool isHvacFlagBatteryCoverOpen(THVACValue *hvacValue);

  static void setHvacSetpointTemperatureHeat(THVACValue *hvacValue,
                                             int16_t setpointTemperatureHeat);
  static void setHvacSetpointTemperatureCool(THVACValue *hvacValue,
                                            int16_t setpointTemperatureCool);

  THVACValue *getValueHvac();
  static bool isHvacValueValid(THVACValue *hvacValue);

  virtual bool isExtended() const;
  bool isUpdateReady() const;
  int getChannelNumber() const;
  _supla_int_t getChannelType() const;

  void setType(_supla_int_t type);
  // setDefault and setDefaultFunction are the same methods.
  // Second was added for better readability
  void setDefault(_supla_int_t value);
  void setDefaultFunction(_supla_int_t function);
  int32_t getDefaultFunction() const;
  bool isFunctionValid(int32_t function) const;
  void setFlag(uint64_t flag);
  void unsetFlag(uint64_t flag);
  uint64_t getFlags() const;
  void setFuncList(_supla_int_t functions);
  _supla_int_t getFuncList() const;
  void addToFuncList(_supla_int_t function);
  void removeFromFuncList(_supla_int_t function);
  void setActionTriggerCaps(_supla_int_t caps);
  _supla_int_t getActionTriggerCaps();

  void setValidityTimeSec(uint32_t timeSec);
  void setUpdateReady();
  void clearUpdateReady();
  virtual void sendUpdate();
  virtual TSuplaChannelExtendedValue *getExtValue();
  // Returns true when value was properly converted to EM value.
  // "out" has to be valid pointer to allocated structure.
  virtual bool getExtValueAsElectricityMeter(
      TElectricityMeter_ExtendedValue_V2 *out);
  void setCorrection(double correction, bool forSecondaryValue = false);
  bool isSleepingEnabled();
  bool isWeeklyScheduleAvailable();

  // Returns true if channel is battery powered (for channel state info)
  bool isBatteryPoweredFieldEnabled() const;
  bool isBatteryPowered() const;
  // Returns battery level
  uint8_t getBatteryLevel() const;
  // Sets battery level. Setting to 0..100 range will make isBatteryPowered
  // return true
  void setBatteryLevel(int level);

  // sets battery powered flag (internally use 101 and 102 battery level value)
  void setBatteryPowered(bool);

  // Sets bridge signal strength. Allowed values are 0..100, or 255 to disable
  void setBridgeSignalStrength(unsigned char level);
  uint8_t getBridgeSignalStrength() const;
  bool isBridgeSignalStrengthAvailable() const;

  void requestChannelConfig();

  void setInitialCaption(const char *caption);
  bool isInitialCaptionSet() const;
  const char* getInitialCaption() const;

  void setDefaultIcon(uint8_t iconId);
  uint8_t getDefaultIcon() const;

  static uint32_t lastCommunicationTimeMs;
  void fillRawValue(void *value);
  int8_t *getValuePtr();

  void setSubDeviceId(uint8_t subDeviceId);
  uint8_t getSubDeviceId() const;

  bool isRollerShutterRelayType() const;

 protected:
  static Channel *firstPtr;
  Channel *nextPtr = nullptr;

  char *initialCaption = nullptr;

  uint64_t channelFlags = 0;
  uint32_t functionsBitmap = 0;
  uint32_t validityTimeSec = 0;

  int16_t channelNumber = -1;

  uint16_t defaultFunction =
      0;  // function in proto use 32 bit, but there are no functions defined so
          // far that use more than 16 bits

  bool valueChanged = false;
  bool channelConfig = false;

  uint8_t batteryLevel = 255;          // 0 - 100%; 255 - not used
  uint8_t batteryPowered = 0;  // 0 - not used, 1 - true, 2 - false
  unsigned char bridgeSignalStrength = 255;  // 0 - 100%; 255 - not used

  // registration parameter
  ChannelType channelType = ChannelType::NOT_SET;

  uint8_t offline = 0;
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
