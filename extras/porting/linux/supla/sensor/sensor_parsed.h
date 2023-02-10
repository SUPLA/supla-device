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

#include <supla/parser/parser.h>

#include <supla-common/proto.h>
#include <map>
#include <string>
#include <vector>

namespace Supla {
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
  void setOnValues(const std::vector<int> &onValues);

 protected:
  double getParameterValue(const std::string &parameter);

  Supla::Parser::Parser *parser = nullptr;
  std::map<std::string, std::string> parameterToKey;
  std::map<std::string, double> parameterMultiplier;
  std::vector<int> stateOnValues;
  int id;
};

template <typename T> class SensorParsed : public T, public SensorParsedBase {
 public:
  explicit SensorParsed(Supla::Parser::Parser *);

  void handleGetChannelState(TDSC_ChannelState *channelState) override;
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
    }
    if (T::getChannel()) {
      T::getChannel()->setBatteryLevel(batteryLevel);
    }
    if (T::getSecondaryChannel()) {
      T::getSecondaryChannel()->setBatteryLevel(batteryLevel);
    }
  }

  T::handleGetChannelState(channelState);
}

};  // namespace Sensor
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SENSOR_SENSOR_PARSED_H_
