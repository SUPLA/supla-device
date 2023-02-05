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

#ifndef SRC_SUPLA_CHANNEL_H_
#define SRC_SUPLA_CHANNEL_H_

#include <stdint.h>

#include "supla-common/proto.h"
#include "local_action.h"

namespace Supla {

class Channel : public LocalAction {
 public:
  Channel();
  virtual ~Channel();

  void setNewValue(double dbl);
  void setNewValue(double temp, double humi);
  void setNewValue(_supla_int_t value);
  void setNewValue(bool value);
  void setNewValue(const TElectricityMeter_ExtendedValue_V2 &emValue);
  void setNewValue(uint8_t red,
                   uint8_t green,
                   uint8_t blue,
                   uint8_t colorBrightness,
                   uint8_t brightness);
  void setNewValue(unsigned _supla_int64_t value);
  void setNewValue(const TDSC_RollerShutterValue &value);
  bool setNewValue(char *newValue);

  double getValueDouble();
  double getValueDoubleFirst();
  double getValueDoubleSecond();
  _supla_int_t getValueInt32();
  unsigned _supla_int64_t getValueInt64();
  bool getValueBool();
  uint8_t getValueRed();
  uint8_t getValueGreen();
  uint8_t getValueBlue();
  uint8_t getValueColorBrightness();
  uint8_t getValueBrightness();

  void setHvacIsOn(uint8_t isOn);
  void setHvacMode(uint8_t mode);
  void setHvacSetpointTemperatureMin(int16_t setpointTemperatureMin);
  void setHvacSetpointTemperatureMax(int16_t setpointTemperatureMax);
  void clearHvacSetpointTemperatureMin();
  void clearHvacSetpointTemperatureMax();
  void setHvacFlags(uint16_t alarmsAndFlags);
  void setHvacFlagSetpointTemperatureMinSet(bool value);
  void setHvacFlagSetpointTemperatureMaxSet(bool value);

  uint8_t getHvacIsOn();
  uint8_t getHvacMode();
  int16_t getHvacSetpointTemperatureMin();
  int16_t getHvacSetpointTemperatureMax();
  uint16_t getHvacFlags();
  bool isHvacFlagSetpointTemperatureMinSet();
  bool isHvacFlagSetpointTemperatureMaxSet();

  THVACValue *getValueHvac();

  virtual bool isExtended();
  bool isUpdateReady();
  int getChannelNumber();
  _supla_int_t getChannelType();

  void setType(_supla_int_t type);
  void setDefault(_supla_int_t value);
  int32_t getDefaultFunction() const;
  void setFlag(_supla_int_t flag);
  void unsetFlag(_supla_int_t flag);
  _supla_int_t getFlags();
  void setFuncList(_supla_int_t functions);
  _supla_int_t getFuncList() const;
  void setActionTriggerCaps(_supla_int_t caps);
  _supla_int_t getActionTriggerCaps();

  void setValidityTimeSec(unsigned _supla_int_t);
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
  bool isBatteryPowered();
  // Returns battery level
  unsigned char getBatteryLevel();
  // Sets battery level. Setting to 0..100 range will make isBatteryPowered
  // return true
  void setBatteryLevel(unsigned char level);

  void requestChannelConfig();

  static uint64_t lastCommunicationTimeMs;
  static TDS_SuplaRegisterDevice_F reg_dev;

 protected:
  void setUpdateReady();

  bool valueChanged;
  bool channelConfig;
  int channelNumber;
  unsigned _supla_int_t validityTimeSec;
  unsigned char batteryLevel = 255;    // 0 - 100%; 255 - not used
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNEL_H_
