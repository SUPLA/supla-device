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

#include <string.h>

#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla-common/srpc.h>
#include <supla/tools.h>
#include <supla/events.h>
#include <supla/correction.h>

#include "channel.h"

namespace Supla {

uint32_t Channel::lastCommunicationTimeMs = 0;
TDS_SuplaRegisterDevice_E Channel::reg_dev;

Channel::Channel() : valueChanged(false), channelConfig(false),
  channelNumber(-1), validityTimeSec(0) {
  if (reg_dev.channel_count < SUPLA_CHANNELMAXCOUNT) {
    channelNumber = reg_dev.channel_count;

    memset(&reg_dev.channels[channelNumber], 0,
        sizeof(reg_dev.channels[channelNumber]));
    reg_dev.channels[channelNumber].Number = channelNumber;

    reg_dev.channel_count++;
  } else {
// TODO(klew): add status CHANNEL_LIMIT_EXCEEDED
  }

  setFlag(SUPLA_CHANNEL_FLAG_CHANNELSTATE);
}

Channel::~Channel() {
  if (reg_dev.channel_count != 0) {
    reg_dev.channel_count--;
  }
}

void Channel::setNewValue(double dbl) {
  bool skipCorrection = false;
  if (getChannelType() == SUPLA_CHANNELTYPE_THERMOMETER) {
    if (dbl <= -273) {
      skipCorrection = true;
    }
  }

  if (!skipCorrection) {
    // Apply channel value correction
    dbl += Correction::get(getChannelNumber());
  }

  char newValue[SUPLA_CHANNELVALUE_SIZE];
  if (sizeof(double) == 8) {
    memcpy(newValue, &dbl, 8);
  } else if (sizeof(double) == 4) {
    float2DoublePacked(dbl, reinterpret_cast<uint8_t *>(newValue));
  }
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d.%d", channelNumber,
        static_cast<int>(dbl), static_cast<int>(dbl*100)%100);
  }
}

void Channel::setNewValue(double temp, double humi) {
  bool skipTempCorrection = false;
  bool skipHumiCorrection = false;
  if (getChannelType() == SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR
      || getChannelType() == SUPLA_CHANNELTYPE_HUMIDITYSENSOR) {
    if (temp <= -273) {
      skipTempCorrection = true;
    }
    if (humi < 0) {
      skipHumiCorrection = true;
    }
  }

  if (!skipTempCorrection) {
    // Apply channel value corrections
    temp += Correction::get(getChannelNumber());
  }

  if (!skipHumiCorrection) {
    double humiCorr = Correction::get(getChannelNumber(), true);
    humi += humiCorr;
    if (humiCorr > 0.01 || humiCorr < -0.01) {
      if (humi < 0) {
        humi = 0;
      }
      if (humi > 100) {
        humi = 100;
      }
    }
  }

  char newValue[SUPLA_CHANNELVALUE_SIZE];
  _supla_int_t t = temp * 1000.00;
  _supla_int_t h = humi * 1000.00;

  memcpy(newValue, &t, 4);
  memcpy(&(newValue[4]), &h, 4);

  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG(
              "Channel(%d) value changed to temp(%f), humi(%f)",
              channelNumber,
              temp,
              humi);
  }
}

void Channel::setNewValue(unsigned _supla_int64_t value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE];

  memset(newValue, 0, SUPLA_CHANNELVALUE_SIZE);

  memcpy(newValue, &value, sizeof(value));
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber,
        static_cast<int>(value));
  }
}

void Channel::setNewValue(_supla_int_t value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE];

  memset(newValue, 0, SUPLA_CHANNELVALUE_SIZE);

  memcpy(newValue, &value, sizeof(value));
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber, value);
  }
}

void Channel::setNewValue(bool value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE];

  memset(newValue, 0, SUPLA_CHANNELVALUE_SIZE);

  newValue[0] = value;
  if (setNewValue(newValue)) {
    if (value) {
      runAction(Supla::ON_TURN_ON);
    } else {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);

    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber, value);
  }
}

void Channel::setNewValue(const TElectricityMeter_ExtendedValue_V2 &emValue) {
  // Prepare standard channel value
  if (sizeof(TElectricityMeter_Value) <= SUPLA_CHANNELVALUE_SIZE) {
    const TElectricityMeter_Measurement *m = nullptr;
    TElectricityMeter_Value v;
    memset(&v, 0, sizeof(TElectricityMeter_Value));

    unsigned _supla_int64_t fae_sum = emValue.total_forward_active_energy[0] +
                                      emValue.total_forward_active_energy[1] +
                                      emValue.total_forward_active_energy[2];

    v.total_forward_active_energy = fae_sum / 1000;

    if (emValue.m_count && emValue.measured_values & EM_VAR_VOLTAGE) {
      m = &emValue.m[emValue.m_count - 1];

      if (m->voltage[0] > 0) {
        v.flags |= EM_VALUE_FLAG_PHASE1_ON;
      }

      if (m->voltage[1] > 0) {
        v.flags |= EM_VALUE_FLAG_PHASE2_ON;
      }

      if (m->voltage[2] > 0) {
        v.flags |= EM_VALUE_FLAG_PHASE3_ON;
      }
    }

    memcpy(reg_dev.channels[channelNumber].value,
           &v,
           sizeof(TElectricityMeter_Value));
    setUpdateReady();
  }
}

bool Channel::setNewValue(char *newValue) {
  if (memcmp(newValue, reg_dev.channels[channelNumber].value, 8) != 0) {
    memcpy(reg_dev.channels[channelNumber].value, newValue, 8);
    setUpdateReady();
    return true;
  }
  return false;
}

void Channel::setType(_supla_int_t type) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].Type = type;
  }
}

void Channel::setDefault(_supla_int_t value) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].Default = value;
  }
}

void Channel::setDefaultFunction(_supla_int_t function) {
  setDefault(function);
}

int32_t Channel::getDefaultFunction() const {
  if (channelNumber >= 0) {
    return reg_dev.channels[channelNumber].Default;
  }
  return 0;
}

void Channel::setFlag(_supla_int_t flag) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].Flags |= flag;
  }
}

void Channel::unsetFlag(_supla_int_t flag) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].Flags &= ~flag;
  }
}

void Channel::setFuncList(_supla_int_t functions) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].FuncList = functions;
  }
}

_supla_int_t Channel::getFuncList() const {
  if (channelNumber >= 0) {
    return reg_dev.channels[channelNumber].FuncList;
  }
  return 0;
}

void Channel::addToFuncList(_supla_int_t function) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].FuncList |= function;
  }
}

void Channel::removeFromFuncList(_supla_int_t function) {
  if (channelNumber >= 0) {
    reg_dev.channels[channelNumber].FuncList &= ~function;
  }
}

_supla_int_t Channel::getFlags() const {
  if (channelNumber >= 0) {
    return reg_dev.channels[channelNumber].Flags;
  }
  return 0;
}

void Channel::setActionTriggerCaps(_supla_int_t caps) {
  SUPLA_LOG_DEBUG("Channel[%d] setting func list: %d", channelNumber,
      caps);
  setFuncList(caps);
}

_supla_int_t Channel::getActionTriggerCaps() {
  return getFuncList();
}

int Channel::getChannelNumber() const {
  return channelNumber;
}

void Channel::clearUpdateReady() {
  valueChanged = false;
}

void Channel::sendUpdate() {
  if (valueChanged) {
    clearUpdateReady();
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->sendChannelValueChanged(channelNumber,
          reg_dev.channels[channelNumber].value,
          offline,
          validityTimeSec);
    }

    // returns null for non-extended channels
    TSuplaChannelExtendedValue *extValue = getExtValue();
    if (extValue) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
          proto != nullptr; proto = proto->next()) {
        proto->sendExtendedChannelValueChanged(channelNumber, extValue);
      }
    }
  }

  // send channel config request if needed
  if (channelConfig) {
    channelConfig = false;
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->getChannelConfig(channelNumber);
    }
  }
}

TSuplaChannelExtendedValue *Channel::getExtValue() {
  return nullptr;
}

bool Channel::getExtValueAsElectricityMeter(
      TElectricityMeter_ExtendedValue_V2 *out) {
  return srpc_evtool_v2_extended2emextended(getExtValue(), out) == 1;
}

void Channel::setUpdateReady() {
  valueChanged = true;
}

bool Channel::isUpdateReady() const {
  return valueChanged || channelConfig;
}

bool Channel::isExtended() const {
  return false;
}

void Channel::setNewValue(const TDSC_RollerShutterValue &value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
  memcpy(newValue, &value, sizeof(TDSC_RollerShutterValue));

  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber,
        value.position);
  }
}

void Channel::setNewValue(uint8_t red,
                   uint8_t green,
                   uint8_t blue,
                   uint8_t colorBrightness,
                   uint8_t brightness) {
  char newValue[SUPLA_CHANNELVALUE_SIZE];
  memset(newValue, 0, SUPLA_CHANNELVALUE_SIZE);
  newValue[0] = brightness;
  newValue[1] = colorBrightness;
  newValue[2] = blue;
  newValue[3] = green;
  newValue[4] = red;
  auto prevBright = getValueBrightness();
  auto prevColorBright = getValueColorBrightness();
  auto prevRed = getValueRed();
  auto prevGreen = getValueGreen();
  auto prevBlue = getValueBlue();
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    bool sendTurnOn = false;
    bool sendTurnOff = false;
    if (prevBright == 0 && getValueBrightness() != 0) {
      runAction(ON_DIMMER_TURN_ON);
      sendTurnOn = true;
    }
    if (prevBright != 0 && getValueBrightness() == 0) {
      runAction(ON_DIMMER_TURN_OFF);
      sendTurnOff = true;
    }
    if (prevBright != getValueBrightness()) {
      runAction(ON_DIMMER_BRIGHTNESS_CHANGE);
    }
    if (prevColorBright == 0 && getValueColorBrightness() != 0) {
      runAction(ON_COLOR_TURN_ON);
      if (getValueRed() != 0) {
        runAction(ON_RED_TURN_ON);
      }
      if (getValueGreen() != 0) {
        runAction(ON_GREEN_TURN_ON);
      }
      if (getValueBlue() != 0) {
        runAction(ON_BLUE_TURN_ON);
      }
      sendTurnOn = true;
    }
    if (prevColorBright != 0 && getValueColorBrightness() == 0) {
      runAction(ON_COLOR_TURN_OFF);
      if (prevRed != 0) {
        runAction(ON_RED_TURN_OFF);
      }
      if (prevGreen != 0) {
        runAction(ON_GREEN_TURN_OFF);
      }
      if (prevBlue != 0) {
        runAction(ON_BLUE_TURN_OFF);
      }
      sendTurnOff = true;
    }
    if (prevColorBright != getValueColorBrightness()) {
      runAction(ON_COLOR_BRIGHTNESS_CHANGE);
    }
    if (prevRed == 0 && getValueRed() != 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_RED_TURN_ON);
    }
    if (prevRed != 0 && getValueRed() == 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_RED_TURN_OFF);
    }

    if (prevGreen == 0 && getValueGreen() != 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_GREEN_TURN_ON);
    }
    if (prevGreen != 0 && getValueGreen() == 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_GREEN_TURN_OFF);
    }

    if (prevBlue == 0 && getValueBlue() != 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_BLUE_TURN_ON);
    }
    if (prevBlue != 0 && getValueBlue() == 0 && prevColorBright != 0
        && getValueColorBrightness() != 0) {
      runAction(ON_BLUE_TURN_OFF);
    }

    if (prevRed != getValueRed()) {
      runAction(ON_RED_CHANGE);
    }

    if (prevGreen != getValueGreen()) {
      runAction(ON_GREEN_CHANGE);
    }

    if (prevBlue != getValueBlue()) {
      runAction(ON_BLUE_CHANGE);
    }

    if (sendTurnOn) {
      runAction(ON_TURN_ON);
    }

    if (sendTurnOff) {
      runAction(ON_TURN_OFF);
    }

    SUPLA_LOG_DEBUG(
        "Channel(%d) value changed to RGB(%d, %d, %d), colBr(%d), bright(%d)",
        channelNumber, red, green, blue, colorBrightness, brightness);
  }
}

_supla_int_t Channel::getChannelType() const {
  if (channelNumber >= 0) {
    return reg_dev.channels[channelNumber].Type;
  }
  return -1;
}

double Channel::getValueDouble() {
  double value;
  if (sizeof(double) == 8) {
    memcpy(&value, reg_dev.channels[channelNumber].value, 8);
  } else if (sizeof(double) == 4) {
    value = doublePacked2float(
        reinterpret_cast<uint8_t *>(reg_dev.channels[channelNumber].value));
  }

  return value;
}

double Channel::getValueDoubleFirst() {
  _supla_int_t value;
  memcpy(&value, reg_dev.channels[channelNumber].value, 4);

  return value / 1000.0;
}

double Channel::getValueDoubleSecond() {
  _supla_int_t value;
  memcpy(&value, &(reg_dev.channels[channelNumber].value[4]), 4);

  return value / 1000.0;
}

_supla_int_t Channel::getValueInt32() {
  _supla_int_t value;
  memcpy(&value, reg_dev.channels[channelNumber].value, sizeof(value));
  return value;
}

unsigned _supla_int64_t Channel::getValueInt64() {
  unsigned _supla_int64_t value;
  memcpy(&value, reg_dev.channels[channelNumber].value, sizeof(value));
  return value;
}

bool Channel::getValueBool() {
  return reg_dev.channels[channelNumber].value[0];
}

uint8_t Channel::getValueRed() {
  return reg_dev.channels[channelNumber].value[4];
}

uint8_t Channel::getValueGreen() {
  return reg_dev.channels[channelNumber].value[3];
}

uint8_t Channel::getValueBlue() {
  return reg_dev.channels[channelNumber].value[2];
}

uint8_t Channel::getValueColorBrightness() {
  return reg_dev.channels[channelNumber].value[1];
}

uint8_t Channel::getValueBrightness() {
  return reg_dev.channels[channelNumber].value[0];
}

#define TEMPERATURE_NOT_AVAILABLE -275.0

double Channel::getLastTemperature() {
  switch (getChannelType()) {
    case SUPLA_CHANNELTYPE_THERMOMETER: {
      return getValueDouble();
    }
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR: {
      return getValueDoubleFirst();
    }
    default: {
      return TEMPERATURE_NOT_AVAILABLE;
    }
  }
}

void Channel::setValidityTimeSec(unsigned _supla_int_t timeSec) {
  validityTimeSec = timeSec;
}

bool Channel::isSleepingEnabled() {
  return validityTimeSec > 0;
}

bool Channel::isWeeklyScheduleAvailable() {
  return getFlags() & SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE;
}

void Channel::setCorrection(double correction, bool forSecondaryValue) {
  Correction::add(getChannelNumber(), correction, forSecondaryValue);
}

void Channel::requestChannelConfig() {
  channelConfig = true;
}

bool Channel::isBatteryPowered() {
  return (batteryLevel <= 100);
}

unsigned char Channel::getBatteryLevel() {
  if (isBatteryPowered()) {
    return batteryLevel;
  }
  return 255;
}

void Channel::setBatteryLevel(unsigned char level) {
  batteryLevel = level;
}

void Channel::setHvacIsOn(uint8_t isOn) {
  auto value = getValueHvac();
  if (value != nullptr && value->IsOn != isOn) {
    if (value->IsOn == 0 && isOn != 0) {
      runAction(Supla::ON_TURN_ON);
    } else if (value->IsOn != 0 && isOn == 0) {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);

    setUpdateReady();
    value->IsOn = isOn;
  }
}

void Channel::setHvacMode(uint8_t mode) {
  auto value = getValueHvac();
  if (value != nullptr && value->Mode != mode && mode <= SUPLA_HVAC_MODE_DRY) {
    setUpdateReady();
    value->Mode = mode;
  }
}

THVACValue *Channel::getValueHvac() const {
  if (channelNumber >= 0 && getChannelType() == SUPLA_CHANNELTYPE_HVAC) {
    return &reg_dev.channels[channelNumber].hvacValue;
  }
  return nullptr;
}

void Channel::setHvacSetpointTemperatureCool(int16_t temperature) {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureCool != temperature ||
                           !isHvacFlagSetpointTemperatureCoolSet())) {
    setUpdateReady();
    value->SetpointTemperatureCool = temperature;
    setHvacFlagSetpointTemperatureCoolSet(true);
  }
}

void Channel::setHvacSetpointTemperatureHeat(int16_t temperature) {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureHeat != temperature ||
                           !isHvacFlagSetpointTemperatureHeatSet())) {
    setUpdateReady();
    value->SetpointTemperatureHeat = temperature;
    setHvacFlagSetpointTemperatureHeatSet(true);
  }
}

void Channel::clearHvacSetpointTemperatureCool() {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureCool != 0 ||
                           isHvacFlagSetpointTemperatureCoolSet())) {
    setUpdateReady();
    value->SetpointTemperatureCool = 0;
    setHvacFlagSetpointTemperatureCoolSet(false);
  }
}

void Channel::clearHvacSetpointTemperatureHeat() {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureHeat != 0 ||
                           isHvacFlagSetpointTemperatureHeatSet())) {
    setUpdateReady();
    value->SetpointTemperatureHeat = 0;
    setHvacFlagSetpointTemperatureHeatSet(false);
  }
}

void Channel::setHvacSetpointTemperatureHeat(THVACValue *hvacValue,
                                             int16_t setpointTemperatureHeat) {
  if (hvacValue != nullptr &&
      (hvacValue->SetpointTemperatureHeat != setpointTemperatureHeat ||
       !isHvacFlagSetpointTemperatureHeatSet(hvacValue))) {
    hvacValue->SetpointTemperatureHeat = setpointTemperatureHeat;
    hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
  }
}

void Channel::setHvacSetpointTemperatureCool(THVACValue *hvacValue,
                                             int16_t setpointTemperatureCool) {
  if (hvacValue != nullptr &&
      (hvacValue->SetpointTemperatureCool != setpointTemperatureCool ||
       !isHvacFlagSetpointTemperatureCoolSet(hvacValue))) {
    hvacValue->SetpointTemperatureCool = setpointTemperatureCool;
    hvacValue->Flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  }
}

void Channel::setHvacFlags(uint16_t flags) {
  auto value = getValueHvac();
  if (value != nullptr && value->Flags != flags) {
    setUpdateReady();
    value->Flags = flags;
  }
}

void Channel::setHvacFlagSetpointTemperatureCoolSet(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagSetpointTemperatureCoolSet()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagSetpointTemperatureHeatSet(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagSetpointTemperatureHeatSet()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagHeating(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagHeating()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_HEATING;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_HEATING;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagCooling(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagCooling()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_COOLING;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_COOLING;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagWeeklySchedule(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagWeeklySchedule()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagFanEnabled(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagFanEnabled()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_FAN_ENABLED;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_FAN_ENABLED;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagThermometerError(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagThermometerError()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagClockError(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagClockError()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagCountdownTimer(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagCountdownTimer()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagForcedOffBySensor(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagForcedOffBySensor()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_FORCED_OFF_BY_SENSOR;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_FORCED_OFF_BY_SENSOR;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagCoolSubfunction(enum HvacCoolSubfunctionFlag flag) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && flag != getHvacFlagCoolSubfunction()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (flag == HvacCoolSubfunctionFlag::CoolSubfunction) {
      // cool subfunction is stored as 1
      flags |= SUPLA_HVAC_VALUE_FLAG_COOL;
    } else {
      // heat subfunction and "not used" is stored as 0
      flags &= ~SUPLA_HVAC_VALUE_FLAG_COOL;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagWeeklyScheduleTemporalOverride(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr &&
      value != isHvacFlagWeeklyScheduleTemporalOverride()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE_TEMPORAL_OVERRIDE;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE_TEMPORAL_OVERRIDE;
    }
    setHvacFlags(flags);
  }
}

bool Channel::isHvacFlagSetpointTemperatureCoolSet() {
  return isHvacFlagSetpointTemperatureCoolSet(getValueHvac());
}

bool Channel::isHvacFlagSetpointTemperatureHeatSet() {
  return isHvacFlagSetpointTemperatureHeatSet(getValueHvac());
}

bool Channel::isHvacFlagHeating() {
  return isHvacFlagHeating(getValueHvac());
}

bool Channel::isHvacFlagCooling() {
  return isHvacFlagCooling(getValueHvac());
}

bool Channel::isHvacFlagWeeklySchedule() {
  return isHvacFlagWeeklySchedule(getValueHvac());
}

bool Channel::isHvacFlagFanEnabled() {
  return isHvacFlagFanEnabled(getValueHvac());
}

bool Channel::isHvacFlagThermometerError() {
  return isHvacFlagThermometerError(getValueHvac());
}

bool Channel::isHvacFlagClockError() {
  return isHvacFlagClockError(getValueHvac());
}

bool Channel::isHvacFlagCountdownTimer() {
  return isHvacFlagCountdownTimer(getValueHvac());
}

bool Channel::isHvacFlagForcedOffBySensor() {
  return isHvacFlagForcedOffBySensor(getValueHvac());
}

enum HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction() {
  return getHvacFlagCoolSubfunction(getValueHvac());
}

bool Channel::isHvacFlagWeeklyScheduleTemporalOverride() {
  return isHvacFlagWeeklyScheduleTemporalOverride(getValueHvac());
}

bool Channel::isHvacFlagSetpointTemperatureHeatSet(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
  }
  return false;
}

bool Channel::isHvacFlagSetpointTemperatureCoolSet(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  }
  return false;
}

bool Channel::isHvacFlagHeating(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_HEATING;
  }
  return false;
}

bool Channel::isHvacFlagCooling(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_COOLING;
  }
  return false;
}

bool Channel::isHvacFlagWeeklySchedule(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
  }
  return false;
}

bool Channel::isHvacFlagFanEnabled(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_FAN_ENABLED;
  }
  return false;
}

bool Channel::isHvacFlagThermometerError(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
  }
  return false;
}

bool Channel::isHvacFlagClockError(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;
  }
  return false;
}

bool Channel::isHvacFlagCountdownTimer(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER;
  }
  return false;
}

bool Channel::isHvacFlagForcedOffBySensor(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_FORCED_OFF_BY_SENSOR;
  }
  return false;
}

enum HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction(
    THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return (hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_COOL
                ? HvacCoolSubfunctionFlag::CoolSubfunction
                : HvacCoolSubfunctionFlag::HeatSubfunctionOrNotUsed);
  }
  return HvacCoolSubfunctionFlag::HeatSubfunctionOrNotUsed;
}

bool Channel::isHvacFlagWeeklyScheduleTemporalOverride(THVACValue *value) {
  if (value != nullptr) {
    return value->Flags &
           SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE_TEMPORAL_OVERRIDE;
  }
  return false;
}

uint8_t Channel::getHvacIsOn() {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->IsOn;
  }
  return 0;
}

uint8_t Channel::getHvacMode() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->Mode;
  }
  return 0;
}

const char *Channel::getHvacModeCstr(int mode) const {
  if (mode == -1) {
    mode = getHvacMode();
  }
  switch (mode) {
    case SUPLA_HVAC_MODE_NOT_SET:
      return "NOT SET";
    case SUPLA_HVAC_MODE_OFF:
      return "OFF";
    case SUPLA_HVAC_MODE_HEAT:
      return "HEAT";
    case SUPLA_HVAC_MODE_COOL:
      return "COOL";
    case SUPLA_HVAC_MODE_AUTO:
      return "AUTO";
    case SUPLA_HVAC_MODE_FAN_ONLY:
      return "FAN ONLY";
    case SUPLA_HVAC_MODE_DRY :
      return "DRY";
    case SUPLA_HVAC_MODE_CMD_TURN_ON:
      return "CMD TURN ON";
    case SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE:
      return "CMD WEEKLY SCHEDULE";
    case SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL:
      return "CMD SWITCH TO MANUAL";

    default:
      return "UNKNOWN";
  }
}

int16_t Channel::getHvacSetpointTemperatureCool() {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->SetpointTemperatureCool;
  }
  return INT16_MIN;
}

int16_t Channel::getHvacSetpointTemperatureHeat() {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->SetpointTemperatureHeat;
  }
  return INT16_MIN;
}

uint16_t Channel::getHvacFlags() {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->Flags;
  }
  return 0;
}

void Channel::setOffline() {
  if (offline == false) {
    offline = true;
    setUpdateReady();
  }
}

void Channel::setOnline() {
  if (offline == true) {
    offline = false;
    setUpdateReady();
  }
}

}  // namespace Supla
