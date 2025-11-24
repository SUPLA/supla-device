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

#include "channel.h"

#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla-common/srpc.h>
#include <supla/tools.h>
#include <supla/events.h>
#include <supla/correction.h>
#include <math.h>
#include <supla/device/register_device.h>

#include <string.h>

#define CHANNEL_SEND_VALUE           (1 << 0)
#define CHANNEL_SEND_GET_CONFIG      (1 << 1)
#define CHANNEL_SEND_STATE_INFO      (1 << 2)
#define CHANNEL_SEND_INITIAL_CAPTION (1 << 3)

using Supla::Channel;

Channel *Channel::firstPtr = nullptr;

#ifdef SUPLA_TEST
// Method used in tests to restore default values for static members
void Supla::Channel::resetToDefaults() {
  Supla::RegisterDevice::resetToDefaults();
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
  if (channelType == ChannelType::THERMOMETER) {
    // for thermometer value must be greater than -273
    if (dbl > -273) {
      // Apply channel value correction
      dbl += Correction::get(getChannelNumber());
      if (dbl < -273) {
        dbl = -273;
      }
    }
  } else {
    // For all other channels
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
  // 2x Float is used only on humidity and humidity&temperature
  if (channelType == ChannelType::HUMIDITYSENSOR ||
      channelType == ChannelType::HUMIDITYANDTEMPSENSOR) {
    if (temp > -273) {
      // Apply channel value corrections
      temp += Correction::get(getChannelNumber());
      if (temp < -273) {
        temp = -273;
      }
    }
    if (humi >= 0) {
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
  }

  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
  int32_t t = temp * 1000.00;
  int32_t h = humi * 1000.00;

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
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};

  memcpy(newValue, &value, sizeof(value));
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber,
        static_cast<int>(value));
  }
}

void Channel::setNewValue(int32_t value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};

  memcpy(newValue, &value, sizeof(value));
  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d", channelNumber, value);
  }
}

void Channel::setNewValue(bool newValueState) {
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
  static_assert(sizeof(newValue) == sizeof(value));

  memcpy(newValue, &value, sizeof(value));

  newValue[0] = newValueState;
  if (setNewValue(newValue)) {
    if (getValueBool()) {
      runAction(Supla::ON_TURN_ON);
    } else {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);

    SUPLA_LOG_DEBUG(
        "Channel(%d) value changed to %d", channelNumber, newValueState);
  }
}

void Channel::setNewValue(
    const TElectricityMeter_ExtendedValue_V3 &emExtValue) {
  // Prepare standard channel value
  if (sizeof(TElectricityMeter_Value) <= SUPLA_CHANNELVALUE_SIZE) {
    const TElectricityMeter_Measurement *m = nullptr;
    union {
      char rawValue[SUPLA_CHANNELVALUE_SIZE] = {};
      TElectricityMeter_Value emValue;
    };

    uint64_t fae_sum =
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
    setSendValue();
  }
}

bool Channel::setNewValue(const char *newValue) {
  if (memcmp(value, newValue, SUPLA_CHANNELVALUE_SIZE) != 0) {
    memcpy(value, newValue, SUPLA_CHANNELVALUE_SIZE);
    setSendValue();
    return true;
  }
  return false;
}

void Channel::setType(uint32_t type) {
  channelType = protoTypeToChannelType(type);
}

void Channel::setDefault(uint32_t value) {
  if (value > UINT16_MAX) {
    SUPLA_LOG_ERROR("Channel[%d]: Invalid defaultFunction value %d",
                    channelNumber, value);
    value = 0;
  }

  if (value == defaultFunction) {
    return;
  }

  defaultFunction = value;
  runAction(ON_CHANNEL_FUNCTION_CHANGE);
}

void Channel::setDefaultFunction(uint32_t function) {
  setDefault(function);
}

uint32_t Channel::getDefaultFunction() const {
  return defaultFunction;
}

void Channel::setFlag(uint64_t flag) {
  channelFlags |= flag;
}

void Channel::unsetFlag(uint64_t flag) {
  channelFlags &= ~flag;
}

void Channel::setFuncList(uint32_t functions) {
  functionsBitmap = functions;
}

uint32_t Channel::getFuncList() const {
  return functionsBitmap;
}

void Channel::addToFuncList(uint32_t function) {
  functionsBitmap |= function;
}

void Channel::removeFromFuncList(uint32_t function) {
  functionsBitmap &= ~function;
}

uint64_t Channel::getFlags() const {
  return channelFlags;
}

void Channel::setActionTriggerCaps(uint32_t caps) {
  setFuncList(caps);
}

uint32_t Channel::getActionTriggerCaps() {
  return getFuncList();
}

int Channel::getChannelNumber() const {
  return channelNumber;
}

void Channel::setSendValue() {
  // set changedFiled
  changedFields |= CHANNEL_SEND_VALUE;
}

void Channel::clearSendValue() {
  changedFields &= ~CHANNEL_SEND_VALUE;
}

void Channel::setSendGetConfig() {
  changedFields |= CHANNEL_SEND_GET_CONFIG;
}

void Channel::clearSendGetConfig() {
  changedFields &= ~CHANNEL_SEND_GET_CONFIG;
}

bool Channel::isGetConfigRequested() const {
  return changedFields & CHANNEL_SEND_GET_CONFIG;
}

bool Channel::isValueUpdateReady() const {
  return changedFields & CHANNEL_SEND_VALUE;
}

void Channel::setSendInitialCaption() {
  changedFields |= CHANNEL_SEND_INITIAL_CAPTION;
}

void Channel::clearSendInitialCaption() {
  changedFields &= ~CHANNEL_SEND_INITIAL_CAPTION;
}

bool Channel::isInitialCaptionUpdateReady() const {
  return changedFields & CHANNEL_SEND_INITIAL_CAPTION;
}

void Channel::setSendStateInfo() {
  changedFields |= CHANNEL_SEND_STATE_INFO;
}

void Channel::clearSendStateInfo() {
  changedFields &= ~CHANNEL_SEND_STATE_INFO;
}

bool Channel::isStateInfoUpdateReady() const {
  return changedFields & CHANNEL_SEND_STATE_INFO;
}

void Channel::sendUpdate() {
  if (isValueUpdateReady()) {
    clearSendValue();
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->sendChannelValueChanged(channelNumber,
          value,
          state,
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

  if (isInitialCaptionUpdateReady()) {
    clearSendInitialCaption();
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      if (isInitialCaptionSet()) {
        proto->setInitialCaption(static_cast<uint8_t>(channelNumber),
                                 getInitialCaption());
      }
    }
  }

  // send channel config request if needed
  if (isGetConfigRequested()) {
    clearSendGetConfig();
    for (auto proto = Supla::Protocol::ProtocolLayer::first();
        proto != nullptr; proto = proto->next()) {
      proto->getChannelConfig(channelNumber);
    }
  }

  if (isStateInfoUpdateReady()) {
    clearSendStateInfo();
    SUPLA_LOG_DEBUG("Channel[%d] sending channel state update",
                    getChannelNumber());
    for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
         proto = proto->next()) {
      proto->sendChannelStateResult(0, static_cast<uint8_t>(channelNumber));
    }
  }
}

TSuplaChannelExtendedValue *Channel::getExtValue() {
  return nullptr;
}

bool Channel::getExtValueAsElectricityMeter(
      TElectricityMeter_ExtendedValue_V3 *out) {
  return srpc_evtool_v3_extended2emextended(getExtValue(), out) == 1;
}

bool Channel::isUpdateReady() const {
  return changedFields != 0;
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

void Channel::setNewValue(const TDSC_FacadeBlindValue &value) {
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
  memcpy(newValue, &value, sizeof(TDSC_FacadeBlindValue));

  if (setNewValue(newValue)) {
    runAction(ON_CHANGE);
    runAction(ON_SECONDARY_CHANNEL_CHANGE);
    SUPLA_LOG_DEBUG("Channel(%d) value changed to %d, tilt %d", channelNumber,
        value.position, value.tilt);
  }
}

void Channel::setNewValue(uint8_t red,
                   uint8_t green,
                   uint8_t blue,
                   uint8_t colorBrightness,
                   uint8_t brightness) {
  char newValue[SUPLA_CHANNELVALUE_SIZE] = {};
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

uint32_t Channel::getChannelType() const {
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

bool Channel::isBatteryPowered() const {
  return batteryPowered == 1;
}

bool Channel::isBatteryPoweredFieldEnabled() const {
  return batteryPowered != 0;
}

uint8_t Channel::getBatteryLevel() const {
  if (batteryLevel <= 100) {
    return batteryLevel;
  }
  return 255;
}

void Channel::setBatteryPowered(bool value) {
  uint8_t newValue = value ? 1 : 2;
  if (newValue != batteryPowered) {
    SUPLA_LOG_DEBUG(
        "Channel[%d] battery powered changed to %d", channelNumber, newValue);
    batteryPowered = newValue;
    setSendStateInfo();
  }
}

void Channel::setBatteryLevel(int level) {
  if (level >= 0 && level <= 100) {
    if (level != batteryLevel) {
      SUPLA_LOG_DEBUG(
          "Channel[%d] battery level changed to %d", channelNumber, level);
      batteryLevel = level;
      setSendStateInfo();
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

void Channel::setHvacIsOn(bool isOn) {
  // channel isOn has range 0..100. However internally negative isOn values
  // are used for cooling indication. Here we convert it to 0..100 range
  auto value = getValueHvac();
  if (value != nullptr && (value->IsOn == 1) != isOn) {
    if (value->IsOn == 0 && isOn) {
      runAction(Supla::ON_TURN_ON);
    } else if (value->IsOn != 0 && !isOn) {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);

    value->IsOn = isOn ? 1 : 0;
    setSendValue();
  }
}

void Channel::setHvacIsOnPercent(uint8_t percent) {
  // channel isOn has range 0..100. However internally negative isOn values
  // are used for cooling indication. Here we convert it to 0..100 range
  auto value = getValueHvac();
  if (percent > 100) {
    percent = 100;
  }
  percent += 2;  // proto uses 2..102 for 0..100%
  if (value != nullptr && value->IsOn != percent) {
    // IsOn == 2 -> OFF
    if (value->IsOn == 2 && percent != 2) {
      runAction(Supla::ON_TURN_ON);
    } else if (value->IsOn != 2 && percent == 2) {
      runAction(Supla::ON_TURN_OFF);
    }
    runAction(Supla::ON_CHANGE);

    value->IsOn = percent;
    setSendValue();
  }
}

void Channel::setHvacMode(uint8_t mode) {
  auto value = getValueHvac();
  if (value != nullptr && value->Mode != mode && mode <= SUPLA_HVAC_MODE_DRY) {
    setSendValue();
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

const THVACValue *Channel::getValueHvac() const {
  if (channelType == ChannelType::HVAC) {
     return &hvacValue;
  }
  return nullptr;
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
    setSendValue();
    value->SetpointTemperatureCool = temperature;
    setHvacFlagSetpointTemperatureCoolSet(true);
  }
}

void Channel::setHvacSetpointTemperatureHeat(int16_t temperature) {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureHeat != temperature ||
                           !isHvacFlagSetpointTemperatureHeatSet())) {
    setSendValue();
    value->SetpointTemperatureHeat = temperature;
    setHvacFlagSetpointTemperatureHeatSet(true);
  }
}

void Channel::clearHvacSetpointTemperatureCool() {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureCool != 0 ||
                           isHvacFlagSetpointTemperatureCoolSet())) {
    setSendValue();
    value->SetpointTemperatureCool = 0;
    setHvacFlagSetpointTemperatureCoolSet(false);
  }
}

void Channel::clearHvacSetpointTemperatureHeat() {
  auto value = getValueHvac();
  if (value != nullptr && (value->SetpointTemperatureHeat != 0 ||
                           isHvacFlagSetpointTemperatureHeatSet())) {
    setSendValue();
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
    setSendValue();
    value->Flags = flags;
  }
}

void Channel::setHvacFlagSetpointTemperatureCoolSet(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagSetpointTemperatureCoolSet()) {
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
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
    setSendValue();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagCalibrationError(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr && value != isHvacFlagCalibrationError()) {
    SUPLA_LOG_ERROR("HVAC[%d]: Setting calibration error flag %d",
                    getChannelNumber(),
                    value);
    setSendValue();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_CALIBRATION_ERROR;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_CALIBRATION_ERROR;
    }
    setHvacFlags(flags);
  }
}

void Channel::setHvacFlagAntifreezeOverheatActive(bool value) {
  auto hvacValue = getValueHvac();
  if (hvacValue != nullptr &&
      value != isHvacFlagAntifreezeOverheatActive()) {
    setSendValue();
    uint16_t flags = hvacValue->Flags;
    if (value) {
      flags |= SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE;
    } else {
      flags &= ~SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE;
    }
    setHvacFlags(flags);
  }
}

void Channel::clearHvacState() {
  SUPLA_LOG_INFO("HVAC[%d]: Clearing HVAC state for channel",
                 getChannelNumber());
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

bool Channel::isHvacFlagSetpointTemperatureCoolSet() const {
  return isHvacFlagSetpointTemperatureCoolSet(getValueHvac());
}

bool Channel::isHvacFlagSetpointTemperatureHeatSet() const {
  return isHvacFlagSetpointTemperatureHeatSet(getValueHvac());
}

bool Channel::isHvacFlagHeating() const {
  return isHvacFlagHeating(getValueHvac());
}

bool Channel::isHvacFlagCooling() const {
  return isHvacFlagCooling(getValueHvac());
}

bool Channel::isHvacFlagWeeklySchedule() const {
  return isHvacFlagWeeklySchedule(getValueHvac());
}

bool Channel::isHvacFlagFanEnabled() const {
  return isHvacFlagFanEnabled(getValueHvac());
}

bool Channel::isHvacFlagThermometerError() const {
  return isHvacFlagThermometerError(getValueHvac());
}

bool Channel::isHvacFlagClockError() const {
  return isHvacFlagClockError(getValueHvac());
}

bool Channel::isHvacFlagCountdownTimer() const {
  return isHvacFlagCountdownTimer(getValueHvac());
}

bool Channel::isHvacFlagForcedOffBySensor() const {
  return isHvacFlagForcedOffBySensor(getValueHvac());
}

enum Supla::HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction()
    const {
  return getHvacFlagCoolSubfunction(getValueHvac());
}

bool Channel::isHvacFlagWeeklyScheduleTemporalOverride() const {
  return isHvacFlagWeeklyScheduleTemporalOverride(getValueHvac());
}

bool Channel::isHvacFlagBatteryCoverOpen() const {
  return isHvacFlagBatteryCoverOpen(getValueHvac());
}

bool Channel::isHvacFlagCalibrationError() const {
  return isHvacFlagCalibrationError(getValueHvac());
}

bool Channel::isHvacFlagAntifreezeOverheatActive() const {
  return isHvacFlagAntifreezeOverheatActive(getValueHvac());
}

bool Channel::isHvacFlagSetpointTemperatureHeatSet(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
  }
  return false;
}

bool Channel::isHvacFlagSetpointTemperatureCoolSet(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_COOL_SET;
  }
  return false;
}

bool Channel::isHvacFlagHeating(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_HEATING;
  }
  return false;
}

bool Channel::isHvacFlagCooling(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_COOLING;
  }
  return false;
}

bool Channel::isHvacFlagWeeklySchedule(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
  }
  return false;
}

bool Channel::isHvacFlagFanEnabled(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_FAN_ENABLED;
  }
  return false;
}

bool Channel::isHvacFlagThermometerError(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
  }
  return false;
}

bool Channel::isHvacFlagClockError(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;
  }
  return false;
}

bool Channel::isHvacFlagCountdownTimer(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_COUNTDOWN_TIMER;
  }
  return false;
}

bool Channel::isHvacFlagForcedOffBySensor(const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags & SUPLA_HVAC_VALUE_FLAG_FORCED_OFF_BY_SENSOR;
  }
  return false;
}

enum Supla::HvacCoolSubfunctionFlag Channel::getHvacFlagCoolSubfunction(
    const THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return (hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_COOL
                ? HvacCoolSubfunctionFlag::CoolSubfunction
                : HvacCoolSubfunctionFlag::HeatSubfunctionOrNotUsed);
  }
  return HvacCoolSubfunctionFlag::HeatSubfunctionOrNotUsed;
}

bool Channel::isHvacFlagWeeklyScheduleTemporalOverride(
    const THVACValue *value) {
  if (value != nullptr) {
    return value->Flags &
           SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE_TEMPORAL_OVERRIDE;
  }
  return false;
}

bool Channel::isHvacFlagBatteryCoverOpen(const THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_BATTERY_COVER_OPEN;
  }
  return false;
}

bool Channel::isHvacFlagCalibrationError(const THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_CALIBRATION_ERROR;
  }
  return false;
}

bool Channel::isHvacFlagAntifreezeOverheatActive(const THVACValue *hvacValue) {
  if (hvacValue != nullptr) {
    return hvacValue->Flags & SUPLA_HVAC_VALUE_FLAG_ANTIFREEZE_OVERHEAT_ACTIVE;
  }
  return false;
}

uint8_t Channel::getHvacIsOnRaw() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->IsOn;
  }
  return false;
}

bool Channel::getHvacIsOnBool() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->IsOn == 1 || value->IsOn > 2;
  }
  return false;
}

uint8_t Channel::getHvacIsOnPercent() const {
  auto value = getValueHvac();
  if (value != nullptr && value->IsOn > 2 && value->IsOn <= 102) {
    return value->IsOn - 2;
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

int16_t Channel::getHvacSetpointTemperatureCool() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->SetpointTemperatureCool;
  }
  return INT16_MIN;
}

int16_t Channel::getHvacSetpointTemperatureHeat() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->SetpointTemperatureHeat;
  }
  return INT16_MIN;
}

uint16_t Channel::getHvacFlags() const {
  auto value = getValueHvac();
  if (value != nullptr) {
    return value->Flags;
  }
  return 0;
}

void Channel::setStateOffline() {
  if (state != SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE) {
    SUPLA_LOG_DEBUG("Channel[%d] go offline", channelNumber);
    state = SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE;
    setSendValue();
  }
}

void Channel::setStateOnline() {
  if (state != SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE) {
    SUPLA_LOG_DEBUG("Channel[%d] go online", channelNumber);
    state = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE;
    setSendValue();
  }
}

void Channel::setStateOnlineAndNotAvailable() {
  if (state != SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE) {
    SUPLA_LOG_DEBUG("Channel[%d] is online and NOT available", channelNumber);
    state = SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
    setSendValue();
  }
}

void Channel::setStateOfflineRemoteWakeupNotSupported() {
  if (state != SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED) {
    SUPLA_LOG_DEBUG("Channel[%d] is offline (remote wakeup not supported)",
                    channelNumber);
    state = SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED;
    setSendValue();
  }
}

void Channel::setStateFirmwareUpdateOngoing() {
  if (state != SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING) {
    SUPLA_LOG_DEBUG("Channel[%d] is online (firmware update ongoing)",
                    channelNumber);
    state = SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING;
    setSendValue();
  }
}

bool Channel::isStateOnline() const {
  return state == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE ||
         state == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE ||
         state == SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING;
}

bool Channel::isStateOnlineAndNotAvailable() const {
  return state == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE;
}

bool Channel::isStateOfflineRemoteWakeupNotSupported() const {
  return state ==
         SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED;
}

bool Channel::isStateFirmwareUpdateOngoing() const {
  return state == SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING;
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
  clearSendValue();
  memset(deviceChannelStruct, 0, sizeof(TDS_SuplaDeviceChannel_D));

  deviceChannelStruct->Number = getChannelNumber();
  deviceChannelStruct->Type = getChannelType();
  deviceChannelStruct->FuncList = getFuncList();  // also sets ActionTriggerCaps
  deviceChannelStruct->Default = getDefaultFunction();
  deviceChannelStruct->Flags = getFlags();
  deviceChannelStruct->Offline = state;
  deviceChannelStruct->ValueValidityTimeSec = validityTimeSec;
  deviceChannelStruct->DefaultIcon = getDefaultIcon();
  memcpy(deviceChannelStruct->value, value, SUPLA_CHANNELVALUE_SIZE);
  // on some ESP platforms, printf functions for 64 bits is not available
  // so we have to print it as two separate 32 bit values 0x%X%08X
  SUPLA_LOG_VERBOSE(
      "CH[%i], type: %d, FuncList: 0x%X, function: %d, flags: 0x%X%08X, "
      "%s, validityTimeSec: %d, icon: %d, "
      "value: "
      "[%02x %02x %02x %02x %02x %02x %02x %02x]",
      getChannelNumber(),
      getChannelType(),
      getFuncList(),
      getDefaultFunction(),
      PRINTF_UINT64_HEX(getFlags()),
      state == 0   ? "online"
      : state == 1 ? "offline"
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
  clearSendValue();
  memset(deviceChannelStruct, 0, sizeof(TDS_SuplaDeviceChannel_E));

  deviceChannelStruct->Number = getChannelNumber();
  deviceChannelStruct->Type = getChannelType();
  deviceChannelStruct->FuncList = getFuncList();  // also sets ActionTriggerCaps
  deviceChannelStruct->Default = getDefaultFunction();
  deviceChannelStruct->Flags = getFlags();
  deviceChannelStruct->Offline = state;
  deviceChannelStruct->ValueValidityTimeSec = validityTimeSec;
  deviceChannelStruct->DefaultIcon = getDefaultIcon();
  deviceChannelStruct->SubDeviceId = getSubDeviceId();
  memcpy(deviceChannelStruct->value, value, SUPLA_CHANNELVALUE_SIZE);
  // uint64_t printf is crashing on ESP32-C2 in method vnsnprintf
  SUPLA_LOG_VERBOSE(
      "CH[%i], subDevId: %d, type: %d, FuncList: 0x%X, function: %d, flags: "
      "0x%X%08X, %s, validityTimeSec: %d, icon: %d, value: "
      "[%02x %02x %02x %02x %02x %02x %02x %02x]",
      getChannelNumber(),
      getSubDeviceId(),
      getChannelType(),
      getFuncList(),
      getDefaultFunction(),
      PRINTF_UINT64_HEX(getFlags()),
      state == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE    ? "online"
      : state == SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE ? "offline"
      : state == SUPLA_CHANNEL_OFFLINE_FLAG_ONLINE_BUT_NOT_AVAILABLE
          ? "online (not available)"
      : state == SUPLA_CHANNEL_OFFLINE_FLAG_OFFLINE_REMOTE_WAKEUP_NOT_SUPPORTED
          ? "offline (remote wakeup not supported)"
      : state == SUPLA_CHANNEL_OFFLINE_FLAG_FIRMWARE_UPDATE_ONGOING
          ? "firmware update ongoing"
          : "UNKNOWN",
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

bool Channel::isFunctionValid(uint32_t function) const {
  if (function == 0) {
    // function 0 -> 'disabled' - assume it is always valid
    return true;
  }

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
        case SUPLA_CHANNELFNC_OPENINGSENSOR_GATEWAY:
        case SUPLA_CHANNELFNC_CONTAINER_LEVEL_SENSOR:
        case SUPLA_CHANNELFNC_FLOOD_SENSOR:
        case SUPLA_CHANNELFNC_BINARY_SENSOR:
        case SUPLA_CHANNELFNC_MOTION_SENSOR: {
          return true;
        }
        default: {
          return false;
        }
      }
    }
    case ChannelType::RELAY: {
      switch (function) {
        case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEGATE;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW;
        }
        case SUPLA_CHANNELFNC_POWERSWITCH: {
          return getFuncList() & SUPLA_CHANNELFNC_POWERSWITCH;
        }
        case SUPLA_CHANNELFNC_LIGHTSWITCH: {
          return getFuncList() & SUPLA_CHANNELFNC_LIGHTSWITCH;
        }
        case SUPLA_CHANNELFNC_STAIRCASETIMER: {
          return getFuncList() & SUPLA_CHANNELFNC_STAIRCASETIMER;
        }
        case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
          return getFuncList() & SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND;
        }
        case SUPLA_CHANNELFNC_TERRACE_AWNING: {
          return getFuncList() & SUPLA_CHANNELFNC_TERRACE_AWNING;
        }
        case SUPLA_CHANNELFNC_PROJECTOR_SCREEN: {
          return getFuncList() & SUPLA_CHANNELFNC_PROJECTOR_SCREEN;
        }
        case SUPLA_CHANNELFNC_CURTAIN: {
          return getFuncList() & SUPLA_CHANNELFNC_CURTAIN;
        }
        case SUPLA_CHANNELFNC_VERTICAL_BLIND: {
          return getFuncList() & SUPLA_CHANNELFNC_VERTICAL_BLIND;
        }
        case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR: {
          return getFuncList() & SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR;
        }
        case SUPLA_CHANNELFNC_PUMPSWITCH: {
          return getFuncList() & SUPLA_CHANNELFNC_PUMPSWITCH;
        }
        case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH: {
          return getFuncList() & SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH;
        }
        default: {
          return false;
        }
      }
    }
    case ChannelType::VALVE_OPENCLOSE: {
      switch (function) {
        case SUPLA_CHANNELFNC_VALVE_OPENCLOSE: {
          return true;
        }
        default: {
          return false;
        }
      }
    }
    case ChannelType::VALVE_PERCENTAGE: {
      switch (function) {
        case SUPLA_CHANNELFNC_VALVE_PERCENTAGE: {
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

bool Channel::isHvacValueValid(const THVACValue *hvacValue) {
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

void Channel::setContainerFillValue(int8_t fillLevel) {
  if (fillLevel > 100) {
    fillLevel = -1;
  }
  if (fillLevel < -1) {
    fillLevel = -1;
  }
  // internally we use 0 for unknown, 1-101 for 0-100%
  ++fillLevel;
  auto container = reinterpret_cast<TContainerChannel_Value *>(value);
  if (container->level == fillLevel) {
    return;
  }
  container->level = fillLevel;
  runAction(ON_CHANGE);
  setSendValue();
  SUPLA_LOG_DEBUG(
      "Container[%d] value changed to %d", channelNumber, fillLevel);
}

void Channel::setContainerAlarm(bool active) {
  auto container = reinterpret_cast<TContainerChannel_Value *>(value);
  if (isContainerAlarmActive() == active) {
    return;
  }
  if (active) {
    container->flags |= CONTAINER_FLAG_ALARM_LEVEL;
  } else {
    container->flags &= ~CONTAINER_FLAG_ALARM_LEVEL;
  }
  setSendValue();
  runAction(active ? ON_CONTAINER_ALARM_ACTIVE : ON_CONTAINER_ALARM_INACTIVE);
}

void Channel::setContainerWarning(bool active) {
  auto container = reinterpret_cast<TContainerChannel_Value *>(value);
  if (isContainerWarningActive() == active) {
    return;
  }
  if (active) {
    container->flags |= CONTAINER_FLAG_WARNING_LEVEL;
  } else {
    container->flags &= ~CONTAINER_FLAG_WARNING_LEVEL;
  }
  setSendValue();
  runAction(active ? ON_CONTAINER_WARNING_ACTIVE
                   : ON_CONTAINER_WARNING_INACTIVE);
}

void Channel::setContainerInvalidSensorState(bool invalid) {
  auto container = reinterpret_cast<TContainerChannel_Value *>(value);
  if (isContainerInvalidSensorStateActive() == invalid) {
    return;
  }
  if (invalid) {
    container->flags |= CONTAINER_FLAG_INVALID_SENSOR_STATE;
  } else {
    container->flags &= ~CONTAINER_FLAG_INVALID_SENSOR_STATE;
  }
  setSendValue();
  runAction(invalid ? ON_CONTAINER_INVALID_SENSOR_STATE_ACTIVE
                    : ON_CONTAINER_INVALID_SENSOR_STATE_INACTIVE);
}

void Channel::setContainerSoundAlarmOn(bool soundAlarmOn) {
  auto container = reinterpret_cast<TContainerChannel_Value *>(value);
  if (isContainerSoundAlarmOn() == soundAlarmOn) {
    return;
  }
  if (soundAlarmOn) {
    container->flags |= CONTAINER_FLAG_SOUND_ALARM_ON;
  } else {
    container->flags &= ~CONTAINER_FLAG_SOUND_ALARM_ON;
  }
  setSendValue();
  runAction(soundAlarmOn ? ON_CONTAINER_SOUND_ALARM_ACTIVE
                         : ON_CONTAINER_SOUND_ALARM_INACTIVE);
}

int8_t Channel::getContainerFillValue() const {
  auto container = reinterpret_cast<const TContainerChannel_Value *>(value);
  if (container->level > 0 && container->level <= 101) {
    return container->level - 1;
  }
  return -1;
}

bool Channel::isContainerAlarmActive() const {
  auto container = reinterpret_cast<const TContainerChannel_Value *>(value);
  return container->flags & CONTAINER_FLAG_ALARM_LEVEL;
}

bool Channel::isContainerWarningActive() const {
  auto container = reinterpret_cast<const TContainerChannel_Value *>(value);
  return container->flags & CONTAINER_FLAG_WARNING_LEVEL;
}

bool Channel::isContainerInvalidSensorStateActive() const {
    auto container = reinterpret_cast<const TContainerChannel_Value *>(value);
  return container->flags & CONTAINER_FLAG_INVALID_SENSOR_STATE;
}


bool Channel::isContainerSoundAlarmOn() const {
  auto container = reinterpret_cast<const TContainerChannel_Value *>(value);
  return container->flags & CONTAINER_FLAG_SOUND_ALARM_ON;
}

void Channel::setValveOpenState(uint8_t openState) {
  if (openState > 100) {
    openState = 100;
  }
  uint8_t newClosed = 0;
  if (getChannelType() == SUPLA_CHANNELTYPE_VALVE_OPENCLOSE) {
    if (openState > 0) {
      newClosed = 0;
    } else {
      newClosed = 1;
    }
  } else if (getChannelType() == SUPLA_CHANNELTYPE_VALVE_PERCENTAGE) {
    newClosed = 100 - openState;
  }

  auto valve = reinterpret_cast<TValve_Value *>(value);
  if (valve->closed == newClosed) {
    return;
  }

  bool runOnOpen = false;
  bool runOnClose = false;
  if (valve->closed == 0) {
    runOnOpen = true;
  } else if (newClosed == 0) {
    runOnClose = true;
  }

  valve->closed = newClosed;
  setSendValue();
  runAction(ON_CHANGE);
  if (runOnOpen) {
    runAction(ON_OPEN);
  }
  if (runOnClose) {
    runAction(ON_CLOSE);
  }
}

void Channel::setValveFloodingFlag(bool active) {
  auto valve = reinterpret_cast<TValve_Value *>(value);
  if (isValveFloodingFlagActive() == active) {
    return;
  }
  SUPLA_LOG_DEBUG("Valve[%d] Flood: %d", channelNumber, active);
  if (active) {
    valve->flags |= SUPLA_VALVE_FLAG_FLOODING;
  } else {
    valve->flags &= ~SUPLA_VALVE_FLAG_FLOODING;
  }
  setSendValue();
  runAction(active ? ON_FLOODING_ACTIVE : ON_FLOODING_INACTIVE);
}

void Channel::setValveManuallyClosedFlag(bool active) {
  auto valve = reinterpret_cast<TValve_Value *>(value);
  if (isValveManuallyClosedFlagActive() == active) {
    return;
  }
  SUPLA_LOG_DEBUG("Valve[%d] ManuallyClosedFlag: %d", channelNumber, active);
  if (active) {
    valve->flags |= SUPLA_VALVE_FLAG_MANUALLY_CLOSED;
  } else {
    valve->flags &= ~SUPLA_VALVE_FLAG_MANUALLY_CLOSED;
  }
  setSendValue();
  runAction(active ? ON_VALVE_MANUALLY_CLOSED_ACTIVE
                   : ON_VALVE_MANUALLY_CLOSED_INACTIVE);
}

void Channel::setValveMotorProblemFlag(bool active) {
  auto valve = reinterpret_cast<TValve_Value *>(value);
  if (isValveMotorProblemFlagActive() == active) {
    return;
  }
  SUPLA_LOG_DEBUG("Valve[%d] MotorProblemFlag: %d", channelNumber, active);
  if (active) {
    valve->flags |= SUPLA_VALVE_FLAG_MOTOR_PROBLEM;
  } else {
    valve->flags &= ~SUPLA_VALVE_FLAG_MOTOR_PROBLEM;
  }
  setSendValue();
  runAction(active ? ON_MOTOR_PROBLEM_ACTIVE : ON_MOTOR_PROBLEM_INACTIVE);
}

uint8_t Channel::getValveOpenState() const {
  auto valve = reinterpret_cast<const TValve_Value *>(value);
  if (getChannelType() == SUPLA_CHANNELTYPE_VALVE_OPENCLOSE) {
    return valve->closed == 0 ? 100 : 0;
  } else if (getChannelType() == SUPLA_CHANNELTYPE_VALVE_PERCENTAGE) {
    if (valve->closed <= 100) {
      return 100 - valve->closed;
    }
  }
  return 0;
}

bool Channel::isValveOpen() const {
  return getValveOpenState() > 0;
}

bool Channel::isValveFloodingFlagActive() const {
  auto valve = reinterpret_cast<const TValve_Value *>(value);
  return valve->flags & SUPLA_VALVE_FLAG_FLOODING;
}

bool Channel::isValveManuallyClosedFlagActive() const {
  auto valve = reinterpret_cast<const TValve_Value *>(value);
  return valve->flags & SUPLA_VALVE_FLAG_MANUALLY_CLOSED;
}

bool Channel::isValveMotorProblemFlagActive() const {
  auto valve = reinterpret_cast<const TValve_Value *>(value);
  return valve->flags & SUPLA_VALVE_FLAG_MOTOR_PROBLEM;
}

void Channel::onRegistered() {
  auto chNumber = getChannelNumber();
  if (chNumber < 0 || chNumber > 255) {
    return;
  }
  if (isInitialCaptionSet()) {
    setSendInitialCaption();
  }
  if (isSleepingEnabled()) {
    setSendValue();
    if (isChannelStateEnabled()) {
      setSendStateInfo();
    }
  }
  if (isChannelStateEnabled() && isStateOnline() &&
      (isBatteryPoweredFieldEnabled() || getBatteryLevel() <= 100)) {
    setSendStateInfo();
  }
}

bool Channel::isChannelStateEnabled() const {
  return getFlags() & SUPLA_CHANNEL_FLAG_CHANNELSTATE;
}

void Channel::setRelayOvercurrentCutOff(bool overcurrent) {
  if (channelType == ChannelType::RELAY) {
    auto relay = reinterpret_cast<TRelayChannel_Value *>(value);
    if ((relay->flags & SUPLA_RELAY_FLAG_OVERCURRENT_RELAY_OFF) !=
        overcurrent) {
      relay->flags = (relay->flags & ~SUPLA_RELAY_FLAG_OVERCURRENT_RELAY_OFF) |
                     (overcurrent ? SUPLA_RELAY_FLAG_OVERCURRENT_RELAY_OFF : 0);
      setSendValue();
    }
  }
}

bool Channel::isRelayOvercurrentCutOff() const {
  if (channelType == ChannelType::RELAY) {
    auto relay = reinterpret_cast<const TRelayChannel_Value *>(value);
    return relay->flags & SUPLA_RELAY_FLAG_OVERCURRENT_RELAY_OFF;
  }
  return false;
}

