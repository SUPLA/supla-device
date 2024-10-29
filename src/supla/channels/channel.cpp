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
#include <math.h>
#include <supla/device/register_device.h>

#include "channel.h"

using Supla::Channel;

uint32_t Channel::lastCommunicationTimeMs = 0;
Channel *Channel::firstPtr = nullptr;

#ifdef SUPLA_TEST
// Method used in tests to restore default values for static members
void Supla::Channel::resetToDefaults() {
  Supla::RegisterDevice::resetToDefaults();
  lastCommunicationTimeMs = 0;
}
#endif

Channel::Channel(int number) {
  if (firstPtr == nullptr) {
    firstPtr = this;
  } else {
    Last()->nextPtr = this;
  }

  if (number == -1) {
    int nextFreeNumber = Supla::RegisterDevice::getNextFreeChannelNumber();
    if (nextFreeNumber == -1) {
      SUPLA_LOG_ERROR("Channel limit exceeded");
      // TODO(klew): add status CHANNEL_LIMIT_EXCEEDED
      return;
    }
    number = nextFreeNumber;
  }

  if (!Supla::RegisterDevice::isChannelNumberFree(number)) {
    SUPLA_LOG_ERROR("Channel with number %d already exists", number);
    return;
  }

  channelNumber = number;
  Supla::RegisterDevice::addChannel(number);

  setFlag(SUPLA_CHANNEL_FLAG_CHANNELSTATE);
}

Channel::~Channel() {
  Supla::RegisterDevice::removeChannel(channelNumber);
  if (initialCaption != nullptr) {
    delete[] initialCaption;
    initialCaption = nullptr;
  }

  if (Begin() == this) {
    firstPtr = next();
    return;
  }

  auto ptr = Begin();
  while (ptr->next() != this) {
    ptr = ptr->next();
  }

  ptr->nextPtr = ptr->next()->next();
}

Channel *Channel::Begin() {
  return firstPtr;
}

Channel *Channel::Last() {
  Channel *ptr = firstPtr;
  while (ptr && ptr->nextPtr) {
    ptr = ptr->nextPtr;
  }
  return ptr;
}

Channel *Channel::GetByChannelNumber(int channelNumber) {
  Channel *ptr = firstPtr;
  while (ptr && ptr->channelNumber != channelNumber) {
    ptr = ptr->nextPtr;
  }
  return ptr;
}

Channel *Channel::next() {
  return nextPtr;
}

bool Channel::setChannelNumber(int newChannelNumber) {
  int oldChannelNumber = channelNumber;

  if (newChannelNumber < 0 || oldChannelNumber < 0) {
    return false;
  }
  if (newChannelNumber == oldChannelNumber) {
    return true;
  }
  if (!Supla::RegisterDevice::isChannelNumberFree(newChannelNumber)) {
    channelNumber = -1;
    auto conflictChannel = GetByChannelNumber(newChannelNumber);
    if (conflictChannel) {
      conflictChannel->setChannelNumber(oldChannelNumber);
    }
  }

  channelNumber = newChannelNumber;
  return true;
}

void Channel::setNewValue(double dbl) {
  bool skipCorrection = false;
  if (channelType == ChannelType::THERMOMETER) {
    if (dbl <= -273) {
      skipCorrection = true;
    }
  }

  if (!skipCorrection) {
    // Apply channel value correction
    dbl += Correction::get(getChannelNumber());
  }

  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
  if (sizeof(double) == 8) {
    memcpy(newValue, &dbl, 8);
  } else if (sizeof(double) == 4) {
    float2DoublePacked(dbl, reinterpret_cast<uint8_t *>(newValue));
  }
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    if (isnan(dbl)) {
      SUPLA_LOG_DEBUG("Channel(%d) value changed to NaN", channelNumber);
    } else {
      SUPLA_LOG_DEBUG("Channel(%d) value changed to %d.%02d", channelNumber,
          static_cast<int>(dbl), abs(static_cast<int>(dbl*100)%100));
    }
  }
}

void Channel::setNewValue(double temp, double humi) {
  bool skipTempCorrection = false;
  bool skipHumiCorrection = false;
  if (channelType == ChannelType::HUMIDITYSENSOR ||
      channelType == ChannelType::HUMIDITYANDTEMPSENSOR) {
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
        "Channel(%d) value changed to temp(%.2f), humi(%.2f)",
        channelNumber,
        temp,
        humi);
  }
}

void Channel::setNewValue(uint64_t value) {
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

void Channel::setNewValue(int32_t value) {
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
    if (getValueBool()) {
      runAction(Supla::ON_TURN_ON);
    } else {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);

    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber, value);
  }
}

void Channel::setNewValue(
    const TElectricityMeter_ExtendedValue_V2 &emExtValue) {
  // Prepare standard channel value
  if (sizeof(TElectricityMeter_Value) <= SUPLA_CHANNELVALUE_SIZE) {
    const TElectricityMeter_Measurement *m = nullptr;
    union {
      char rawValue[SUPLA_CHANNELVALUE_SIZE] = {};
      TElectricityMeter_Value emValue;
    };

    unsigned _supla_int64_t fae_sum =
        emExtValue.total_forward_active_energy[0] +
        emExtValue.total_forward_active_energy[1] +
        emExtValue.total_forward_active_energy[2];

    emValue.total_forward_active_energy = fae_sum / 1000;

    if (emExtValue.m_count && emExtValue.measured_values & EM_VAR_VOLTAGE) {
      m = &emExtValue.m[emExtValue.m_count - 1];

      if (m->voltage[0] > 0) {
        emValue.flags |= EM_VALUE_FLAG_PHASE1_ON;
      }

      if (m->voltage[1] > 0) {
        emValue.flags |= EM_VALUE_FLAG_PHASE2_ON;
      }

      if (m->voltage[2] > 0) {
        emValue.flags |= EM_VALUE_FLAG_PHASE3_ON;
      }
    }

    setNewValue(rawValue);
    setUpdateReady();
  }
}

bool Channel::setNewValue(const char *newValue) {
  if (memcmp(value, newValue, SUPLA_CHANNELVALUE_SIZE) != 0) {
    memcpy(value, newValue, SUPLA_CHANNELVALUE_SIZE);
    setUpdateReady();
    return true;
  }
  return false;
}

void Channel::setType(_supla_int_t type) {
  channelType = protoTypeToChannelType(type);
}

void Channel::setDefault(_supla_int_t value) {
  if (value > UINT16_MAX) {
    SUPLA_LOG_ERROR("Channel[%d]: Invalid defaultFunction value %d",
                    channelNumber, value);
    value = 0;
  }

  defaultFunction = value;
}

void Channel::setDefaultFunction(_supla_int_t function) {
  setDefault(function);
}

int32_t Channel::getDefaultFunction() const {
  return defaultFunction;
}

void Channel::setFlag(uint64_t flag) {
  channelFlags |= flag;
}

void Channel::unsetFlag(uint64_t flag) {
  channelFlags &= ~flag;
}

void Channel::setFuncList(_supla_int_t functions) {
  functionsBitmap = functions;
}

_supla_int_t Channel::getFuncList() const {
  return functionsBitmap;
}

void Channel::addToFuncList(_supla_int_t function) {
  functionsBitmap |= function;
}

void Channel::removeFromFuncList(_supla_int_t function) {
  functionsBitmap &= ~function;
}

uint64_t Channel::getFlags() const {
  return channelFlags;
}

void Channel::setActionTriggerCaps(_supla_int_t caps) {
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
          value,
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
  return channelTypeToProtoType(channelType);
}

double Channel::getValueDouble() {
  double valueDouble;
  if (sizeof(double) == 8) {
    memcpy(&valueDouble, value, 8);
  } else if (sizeof(double) == 4) {
    valueDouble = doublePacked2float(reinterpret_cast<uint8_t *>(value));
  }

  return valueDouble;
}

double Channel::getValueDoubleFirst() {
  int32_t valueInt = 0;
  memcpy(&valueInt, value, 4);

  return valueInt / 1000.0;
}

double Channel::getValueDoubleSecond() {
  int32_t valueInt = 0;
  memcpy(&valueInt, value + 4, 4);

  return valueInt / 1000.0;
}

int32_t Channel::getValueInt32() {
  int32_t valueInt = 0;
  memcpy(&valueInt, value, sizeof(valueInt));
  return valueInt;
}

uint64_t Channel::getValueInt64() {
  uint64_t valueInt = 0;
  memcpy(&valueInt, value, sizeof(valueInt));
  return valueInt;
}

bool Channel::getValueBool() {
  return value[0] != 0;
}

uint8_t Channel::getValueRed() {
  return value[4];
}

uint8_t Channel::getValueGreen() {
  return value[3];
}

uint8_t Channel::getValueBlue() {
  return value[2];
}

uint8_t Channel::getValueColorBrightness() {
  return value[1];
}

uint8_t Channel::getValueBrightness() {
  return value[0];
}

uint8_t Channel::getValueClosingPercentage() const {
  return value[0] >= 0 ? value[0] : 0;
}

bool Channel::getValueIsCalibrating() const {
  return value[0] == -1;
}

uint8_t Channel::getValueTilt() const {
  return value[1] >= 0 ? value[1] : 0;
}

#define TEMPERATURE_NOT_AVAILABLE -275.0

double Channel::getLastTemperature() {
  switch (channelType) {
    case ChannelType::THERMOMETER: {
      return getValueDouble();
    }
    case ChannelType::HUMIDITYANDTEMPSENSOR: {
      return getValueDoubleFirst();
    }
    default: {
      return TEMPERATURE_NOT_AVAILABLE;
    }
  }
}

void Channel::setValidityTimeSec(uint32_t timeSec) {
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

bool Channel::isBatteryPowered() const {
  return batteryPowered == 1;
}

bool Channel::isBatteryPoweredFieldEnabled() const {
  return batteryPowered;
}

uint8_t Channel::getBatteryLevel() const {
  if (batteryLevel <= 100) {
    return batteryLevel;
  }
  return 255;
}

void Channel::setBatteryPowered(bool value) {
  batteryPowered = value ? 1 : 2;
}

void Channel::setBatteryLevel(int level) {
  if (level >= 0 && level <= 100) {
    batteryLevel = level;
    if (!isBatteryPoweredFieldEnabled()) {
      setBatteryPowered(true);
    }
  }
}

uint8_t Channel::getBridgeSignalStrength() const {
  return bridgeSignalStrength;
}

void Channel::setBridgeSignalStrength(uint8_t strength) {
  bridgeSignalStrength = strength;
}

bool Channel::isBridgeSignalStrengthAvailable() const {
  return bridgeSignalStrength <= 100;
}

void Channel::setHvacIsOn(int8_t isOn) {
  // channel isOn has range 0..100. However internally negative isOn values
  // are used for cooling indication. Here we convert it to 0..100 range
  auto value = getValueHvac();
  if (isOn < 0) {
    isOn = -isOn;
  }
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
    if (mode == SUPLA_HVAC_MODE_OFF) {
      runAction(ON_HVAC_MODE_OFF);
    } else if (mode == SUPLA_HVAC_MODE_HEAT) {
      runAction(ON_HVAC_MODE_HEAT);
    } else if (mode == SUPLA_HVAC_MODE_COOL) {
      runAction(ON_HVAC_MODE_COOL);
    } else if (mode == SUPLA_HVAC_MODE_HEAT_COOL) {
      runAction(ON_HVAC_MODE_HEAT_COOL);
    }
  }
}

THVACValue *Channel::getValueHvac() {
  if (channelType == ChannelType::HVAC) {
     return &hvacValue;
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
    if (value) {
      runAction(ON_HVAC_WEEKLY_SCHEDULE_ENABLED);
    } else {
      runAction(ON_HVAC_WEEKLY_SCHEDULE_DISABLED);
    }
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

void Channel::setHvacFlagBatteryCoverOpen(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagBatteryCoverOpen()) {
    setUpdateReady();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
    }
    setHvacFlags(flags);
  }
}

void Channel::clearHvacState() {
  clearHvacSetpointTemperatureCool();
  clearHvacSetpointTemperatureHeat();
  setHvacMode(SUPLA_HVAC_MODE_OFF);
  setHvacFlagWeeklySchedule(false);
  setHvacFlagHeating(false);
  setHvacFlagCooling(false);
  setHvacIsOn(0);
  setHvacFlagFanEnabled(false);
  setHvacFlagThermometerError(false);
  setHvacFlagClockError(false);
  setHvacFlagCountdownTimer(false);
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

enum Supla::HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction() {
  return getHvacFlagCoolSubfunction(getValueHvac());
}

bool Channel::isHvacFlagWeeklyScheduleTemporalOverride() {
  return isHvacFlagWeeklyScheduleTemporalOverride(getValueHvac());
}

bool Channel::isHvacFlagBatteryCoverOpen() {
  return isHvacFlagBatteryCoverOpen(getValueHvac());
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

enum Supla::HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction(
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

bool Channel::isHvacFlagBatteryCoverOpen(THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
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
  return hvacValue.Mode;
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
    case SUPLA_HVAC_MODE_HEAT_COOL:
      return "HEAT_COOL";
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
  if (offline != 1) {
    SUPLA_LOG_DEBUG("Channel[%d] go offline", channelNumber);
    offline = 1;
    setUpdateReady();
  }
}

void Channel::setOnline() {
  if (offline != 0) {
    SUPLA_LOG_DEBUG("Channel[%d] go online", channelNumber);
    offline = 0;
    setUpdateReady();
  }
}

void Channel::setOnlineAndNotAvailable() {
  if (offline != 2) {
    SUPLA_LOG_DEBUG("Channel[%d] is online and NOT available", channelNumber);
    offline = 2;
    setUpdateReady();
  }
}


bool Channel::isOnline() const {
  return offline != 1;
}

bool Channel::isOnlineAndNotAvailable() const {
  return offline == 2;
}

void Channel::setInitialCaption(const char *caption) {
  if (initialCaption != nullptr) {
    delete [] initialCaption;
    initialCaption = nullptr;
  }
  int len = strnlen(caption, SUPLA_CAPTION_MAXSIZE - 1);
  if (len > 0) {
    initialCaption = new char[len + 1];
    strncpy(initialCaption, caption, len);
    initialCaption[len] = '\0';
    SUPLA_LOG_DEBUG("Channel[%d] initial caption set to '%s'", channelNumber,
                    initialCaption);
  }
}

const char* Channel::getInitialCaption() const {
  return initialCaption;
}

bool Channel::isInitialCaptionSet() const {
  return initialCaption != nullptr;
}

void Channel::setDefaultIcon(uint8_t iconId) {
  defaultIcon = iconId;
}

uint8_t Channel::getDefaultIcon() const {
  return defaultIcon;
}

void Channel::fillDeviceChannelStruct(
    TDS_SuplaDeviceChannel_D *deviceChannelStruct) {
  if (deviceChannelStruct == nullptr) {
    return;
  }
  // this method is called during register device message preparation, so
  // we clear update ready flag in order to not send the same values again
  // after registration
  clearUpdateReady();
  memset(deviceChannelStruct, 0, sizeof(TDS_SuplaDeviceChannel_D));

  deviceChannelStruct->Number = getChannelNumber();
  deviceChannelStruct->Type = getChannelType();
  deviceChannelStruct->FuncList = getFuncList();  // also sets ActionTriggerCaps
  deviceChannelStruct->Default = getDefaultFunction();
  deviceChannelStruct->Flags = getFlags();
  deviceChannelStruct->Offline = offline;
  deviceChannelStruct->ValueValidityTimeSec = validityTimeSec;
  deviceChannelStruct->DefaultIcon = getDefaultIcon();
  memcpy(deviceChannelStruct->value, value, SUPLA_CHANNELVALUE_SIZE);
  SUPLA_LOG_VERBOSE(
      "CH[%i], type: %d, FuncList: 0x%X, function: %d, flags: 0x%llX, "
      "%s, validityTimeSec: %d, icon: %d, "
      "value: "
      "[%02x %02x %02x %02x %02x %02x %02x %02x]",
      getChannelNumber(),
      getChannelType(),
      getFuncList(),
      getDefaultFunction(),
      getFlags(),
      offline == 0   ? "online"
      : offline == 1 ? "offline"
                     : "online (not available)",
      validityTimeSec,
      getDefaultIcon(),
      static_cast<uint8_t>(value[0]),
      static_cast<uint8_t>(value[1]),
      static_cast<uint8_t>(value[2]),
      static_cast<uint8_t>(value[3]),
      static_cast<uint8_t>(value[4]),
      static_cast<uint8_t>(value[5]),
      static_cast<uint8_t>(value[6]),
      static_cast<uint8_t>(value[7]));
}

void Channel::fillDeviceChannelStruct(
    TDS_SuplaDeviceChannel_E *deviceChannelStruct) {
  if (deviceChannelStruct == nullptr) {
    return;
  }
  // this method is called during register device message preparation, so
  // we clear update ready flag in order to not send the same values again
  // after registration
  clearUpdateReady();
  memset(deviceChannelStruct, 0, sizeof(TDS_SuplaDeviceChannel_E));

  deviceChannelStruct->Number = getChannelNumber();
  deviceChannelStruct->Type = getChannelType();
  deviceChannelStruct->FuncList = getFuncList();  // also sets ActionTriggerCaps
  deviceChannelStruct->Default = getDefaultFunction();
  deviceChannelStruct->Flags = getFlags();
  deviceChannelStruct->Offline = offline;
  deviceChannelStruct->ValueValidityTimeSec = validityTimeSec;
  deviceChannelStruct->DefaultIcon = getDefaultIcon();
  deviceChannelStruct->SubDeviceId = getSubDeviceId();
  memcpy(deviceChannelStruct->value, value, SUPLA_CHANNELVALUE_SIZE);
  SUPLA_LOG_VERBOSE(
      "CH[%i], subDevId: %d, type: %d, FuncList: 0x%X, function: %d, flags: "
      "0x%llX, %s, validityTimeSec: %d, icon: %d, value: "
      "[%02x %02x %02x %02x %02x %02x %02x %02x]",
      getChannelNumber(),
      getSubDeviceId(),
      getChannelType(),
      getFuncList(),
      getDefaultFunction(),
      getFlags(),
      offline == 0   ? "online"
      : offline == 1 ? "offline"
                     : "online (not available)",
      validityTimeSec,
      getDefaultIcon(),
      static_cast<uint8_t>(value[0]),
      static_cast<uint8_t>(value[1]),
      static_cast<uint8_t>(value[2]),
      static_cast<uint8_t>(value[3]),
      static_cast<uint8_t>(value[4]),
      static_cast<uint8_t>(value[5]),
      static_cast<uint8_t>(value[6]),
      static_cast<uint8_t>(value[7]));
}

void Channel::fillRawValue(void *valueToFill) {
  if (valueToFill == nullptr) {
    return;
  }
  memcpy(valueToFill, value, SUPLA_CHANNELVALUE_SIZE);
}

int8_t *Channel::getValuePtr() {
  return value;
}

bool Channel::isFunctionValid(int32_t function) const {
  switch (channelType) {
    // TODO(klew): add other functions
    case ChannelType::BINARYSENSOR: {
      switch (function) {
        case SUPLA_CHANNELFNC_NOLIQUIDSENSOR:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_WINDOW:
        case SUPLA_CHANNELFNC_HOTELCARDSENSOR:
        case SUPLA_CHANNELFNC_ALARMARMAMENTSENSOR:
        case SUPLA_CHANNELFNC_MAILSENSOR:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_ROLLERSHUTTER:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_ROOFWINDOW:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_GARAGEDOOR:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_GATE:
        case SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY: {
          return true;
        }
        default: {
          return false;
        }
      }
    }
    default: {
      return true;
    }
  }
}

void Channel::setSubDeviceId(uint8_t subDeviceId) {
  this->subDeviceId = subDeviceId;
}

uint8_t Channel::getSubDeviceId() const {
  return subDeviceId;
}

bool Channel::isHvacValueValid(THVACValue *hvacValue) {
  if (hvacValue == nullptr) {
    return false;
  }
  if (hvacValue->IsOn > 102) {
    return false;
  }
  // last supported mode:
  if (hvacValue->Mode > SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL) {
    return false;
  }
  if (hvacValue->Flags > 0x1FFF) {  // (1ULL << 12)) {
    return false;
  }
  if (hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_COOLING &&
      hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_HEATING) {
    return false;
  }

  return true;
}

bool Channel::isRollerShutterRelayType() const {
  return (functionsBitmap &
          (SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
           SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
           SUPLA_BIT_FUNC_TERRACE_AWNING | SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR |
           SUPLA_BIT_FUNC_CURTAIN | SUPLA_BIT_FUNC_PROJECTOR_SCREEN |
           SUPLA_BIT_FUNC_VERTICAL_BLIND |
           SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND)) != 0;
}
