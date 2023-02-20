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

#include "sensor_parsed.h"

#include <math.h>
#include <supla/control/action_trigger_parsed.h>
#include <supla/log_wrapper.h>
#include <supla/sensor/binary_parsed.h>

#include <algorithm>
#include <utility>

using Supla::Sensor::SensorParsedBase;

std::map<std::string, Supla::Control::ActionTriggerParsed *>
    SensorParsedBase::atMap;

SensorParsedBase::SensorParsedBase(Supla::Parser::Parser *parser)
    : parser(parser) {
  static int instanceCounter = 0;
  id = instanceCounter++;
}

void SensorParsedBase::setMapping(const std::string &parameter,
                                                const std::string &key) {
  parameterToKey[parameter] = key;
  parser->addKey(key, -1);  // ignore index
}

void SensorParsedBase::setMapping(const std::string &parameter,
                                                const int index) {
  std::string key = parameter;
  key += "_";
  key += std::to_string(id);
  parameterToKey[parameter] = key;
  parser->addKey(key, index);
}

void SensorParsedBase::setMultiplier(const std::string &parameter,
                                                   double multiplier) {
  parameterMultiplier[parameter] = multiplier;
}

double SensorParsedBase::getParameterValue(
    const std::string &parameter) {
  double multiplier = 1;
  if (parameterMultiplier.count(parameter)) {
    multiplier = parameterMultiplier[parameter];
  }
  return parser->getValue(parameterToKey[parameter]) * multiplier;
}

bool SensorParsedBase::refreshParserSource() {
  if (parser && parser->refreshParserSource()) {
    return true;
  }
  return false;
}

bool SensorParsedBase::isParameterConfigured(
    const std::string &parameter) {
  return parameterToKey.count(parameter) > 0;
}

int SensorParsedBase::getStateValue() {
  int value = -1;

  if (isParameterConfigured(Supla::Parser::State)) {
    if (refreshParserSource()) {
      double result = getParameterValue(Supla::Parser::State);
      value = round(result);
      if (!parser->isValid()) {
        value = -1;
        setLastValue(value);
      } else {
        setLastValue(value);

        if (!stateOnValues.empty()) {
          // if value is in stateOnValues vector
          if (std::find(stateOnValues.begin(), stateOnValues.end(), value)
              != stateOnValues.end()) {
            value = 1;
          } else {
            if (value != -1) {
              value = 0;
            }
          }
        }
      }
    }
  }
  return value;
}

void SensorParsedBase::setOnValues(const std::vector<int> &onValues) {
  stateOnValues = onValues;
}

bool SensorParsedBase::addAtOnState(const std::vector<int> &onState) {
  if (onState.size() != 2) {
    SUPLA_LOG_ERROR("Invalid on_state vector size: %d, expected 2",
        onState.size());
    return false;
  }
  atOnState[onState[0]] = onState[1];
  return true;
}

bool SensorParsedBase::addAtOnValue(const std::vector<int> &onValue) {
  if (onValue.size() != 2) {
    SUPLA_LOG_ERROR("Invalid on_value vector size: %d, expected 2",
        onValue.size());
    return false;
  }
  atOnValue[onValue[0]] = onValue[1];
  return true;
}

bool SensorParsedBase::addAtOnStateChange(
    const std::vector<int> &onStateChange) {
  if (onStateChange.size() != 3) {
    SUPLA_LOG_ERROR("Invalid on_state_change vector size: %d, expected 3",
        onStateChange.size());
    return false;
  }
  atOnStateChangeFromTo[std::make_pair(onStateChange[0], onStateChange[1])] =
    onStateChange[2];
  return true;
}

bool SensorParsedBase::addAtOnValueChange(
    const std::vector<int> &onValueChange) {
  if (onValueChange.size() != 3) {
    SUPLA_LOG_ERROR("Invalid on_value_change vector size: %d, expected 3",
        onValueChange.size());
    return false;
  }
  atOnValueChangeFromTo[std::make_pair(onValueChange[0], onValueChange[1])] =
    onValueChange[2];
  return true;
}

void SensorParsedBase::setLastState(int newState) {
  if (lastState != newState) {
    auto atIt = atMap.find(atName);
    if (atIt != atMap.end()) {
      auto at = atIt->second;
      if (auto it =
          atOnStateChangeFromTo.find(std::make_pair(lastState, newState));
          it != atOnStateChangeFromTo.end()) {
        at->sendActionTrigger(it->second);
      }
      if (auto it = atOnState.find(newState); it != atOnState.end()) {
        at->sendActionTrigger(it->second);
      }
    }
  }

  lastState = newState;
}

void SensorParsedBase::setAtName(std::string name) {
  atName = name;
}

void SensorParsedBase::setLastValue(int newValue) {
  if (lastValue != newValue) {
    auto atIt = atMap.find(atName);
    if (atIt != atMap.end()) {
      auto at = atIt->second;
      if (auto it =
          atOnValueChangeFromTo.find(std::make_pair(lastValue, newValue));
          it != atOnValueChangeFromTo.end()) {
        at->sendActionTrigger(it->second);
      }
      if (auto it = atOnValue.find(newValue); it != atOnValue.end()) {
        at->sendActionTrigger(it->second);
      }
    }
  }
  lastValue = newValue;
}


void SensorParsedBase::registerActions() {
  if (atName.length() == 0) {
    return;
  }
  auto atIt = atMap.find(atName);
  SUPLA_LOG_INFO("Registering action %s", atName.c_str());
  if (atIt != atMap.end()) {
    auto at = atIt->second;
    for (auto onState : atOnState) {
      at->activateAction(onState.second);
    }
    for (auto onValue : atOnValue) {
      at->activateAction(onValue.second);
    }
    for (auto onStateChange : atOnStateChangeFromTo) {
      at->activateAction(onStateChange.second);
    }
    for (auto onValueChange : atOnValueChangeFromTo) {
      at->activateAction(onValueChange.second);
    }
  } else {
    SUPLA_LOG_ERROR("Cannot find action_trigger with name: %s", atName.c_str());
  }
}

void SensorParsedBase::registerAtName(std::string name,
                                      Supla::Control::ActionTriggerParsed *at) {
  atMap[name] = at;
}
