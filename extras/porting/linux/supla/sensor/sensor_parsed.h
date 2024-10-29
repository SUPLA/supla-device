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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SENSOR_SENSOR_PARSED_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SENSOR_SENSOR_PARSED_H_

#include <supla-common/proto.h>
#include <supla/control/action_trigger.h>
#include <supla/parser/parser.h>

#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace Supla {

namespace Control {
class ActionTriggerParsed;
}  // namespace Control

namespace Sensor {
const char BatteryLevel[] = "battery_level";
const char MultiplierBatteryLevel[] = "multiplier_battery_level";

class SensorParsedBase {
 public:
  explicit SensorParsedBase(Supla::Parser::Parser *);

  void setMapping(const std::string &parameter, const std::string &key);

  void setMapping(const std::string &parameter, const int index);

  void setMultiplier(const std::string &parameter, double multiplier);

  bool refreshParserSource();

  bool isParameterConfigured(const std::string &parameter);

  // Returns -1 on invalid source/parser/value,
  // Otherwise returns >= 0 read from parser.
  int getStateValue();
  void setOnValues(
      const std::vector<std::variant<int, bool, std::string>> &onValues);
  bool addAtOnState(const std::vector<int> &onState);
  bool addAtOnValue(const std::vector<int> &onValue);
  bool addAtOnStateChange(const std::vector<int> &onState);
  bool addAtOnValueChange(const std::vector<int> &onValue);
  void setLastState(int newState);
  void setLastValue(int newValue);
  void setAtName(std::string);
  void registerActions();

  static void registerAtName(std::string,
                             Supla::Control::ActionTriggerParsed *);

  virtual void setInitialCaption(const std::string &caption) = 0;
  virtual Supla::Channel *getChannel() = 0;

 protected:
  double getParameterValue(const std::string &parameter);

  // parser configuration
  int id;
  Supla::Parser::Parser *parser = nullptr;
  std::map<std::string, std::string> parameterToKey;
  std::map<std::string, double> parameterMultiplier;
  std::vector<std::variant<int, bool, std::string>> stateOnValues;

  // action trigger configuration
  std::string atName;
  int lastState = -1;
  int lastValue = -1;
  std::map<int, int> atOnState;  // map<state, event>
  std::map<int, int> atOnValue;
  std::map<std::pair<int, int>, int>
      atOnStateChangeFromTo;  // map<<from, to>, event>
  std::map<std::pair<int, int>, int> atOnValueChangeFromTo;

  static std::map<std::string, Supla::Control::ActionTriggerParsed *> atMap;

  std::variant<int, bool, std::string> getStateParameterValue(
      const std::string &parameter);

  // TODO(klew): add local action
};

template <typename T>
class SensorParsed : public T, public SensorParsedBase {
 public:
  explicit SensorParsed(Supla::Parser::Parser *);

  void handleGetChannelState(TDSC_ChannelState *channelState) override;
  void setInitialCaption(const std::string &caption) override;
  Supla::Channel *getChannel() override;
  const Supla::Channel *getChannel() const override;
};

template <typename T>
SensorParsed<T>::SensorParsed(Supla::Parser::Parser *parser)
    : SensorParsedBase(parser) {
}


template <typename T>
void SensorParsed<T>::handleGetChannelState(TDSC_ChannelState *channelState) {
  unsigned char batteryLevel = 255;
  if (isParameterConfigured(BatteryLevel)) {
    if (refreshParserSource()) {
      batteryLevel = getParameterValue(BatteryLevel);
      if (!parser->isValid()) {
        batteryLevel = 255;
      }
    }
    if (T::getChannel()) {
      T::getChannel()->setBatteryPowered(true);
      if (batteryLevel <= 100) {
        T::getChannel()->setBatteryLevel(batteryLevel);
      }
    }
    if (T::getSecondaryChannel()) {
      T::getSecondaryChannel()->setBatteryPowered(true);
      if (batteryLevel <= 100) {
        T::getSecondaryChannel()->setBatteryLevel(batteryLevel);
      }
    }
  }

  T::handleGetChannelState(channelState);
}

template <typename T>
void SensorParsed<T>::setInitialCaption(const std::string &caption) {
  T::setInitialCaption(caption.c_str());
}

template <typename T>
Supla::Channel *SensorParsed<T>::getChannel() {
  return T::getChannel();
}

template <typename T>
const Supla::Channel *SensorParsed<T>::getChannel() const {
  return T::getChannel();
}

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_SENSOR_PARSED_H_
