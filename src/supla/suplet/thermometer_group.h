/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef SRC_SUPLA_SUPLET_THERMOMETER_GROUP_H_
#define SRC_SUPLA_SUPLET_THERMOMETER_GROUP_H_

#include <stdint.h>
#include <supla/suplet/virtual_channel.h>

#ifndef SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES
#define SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES 8
#endif

namespace Supla {
namespace Suplet {

enum class ThermometerGroupMode : uint8_t {
  Avg = 1,
  Min = 2,
  Max = 3,
};

struct ThermometerGroupConfig {
  uint8_t version = 1;
  ThermometerGroupMode mode = ThermometerGroupMode::Avg;
  uint16_t refreshIntervalMs = 1000;
  uint8_t sourceCount = 0;
  int16_t sourceChannels[SUPLA_SUPLET_THERMOMETER_GROUP_MAX_SOURCES] = {};
};

class ThermometerGroup : public VirtualThermometer {
 public:
  ThermometerGroup(uint8_t subDeviceId,
                   int channelNumber,
                   const ThermometerGroupConfig &config);

  void iterateAlways() override;
  double calculateValue() const;
  const ThermometerGroupConfig &getConfig() const;

 private:
  struct GroupValue {
    bool hasOnlineSource = false;
    bool hasValidValue = false;
    double value = TEMPERATURE_NOT_AVAILABLE;
  };

  static bool isValidTemperature(double value);
  GroupValue calculateGroupValue() const;

  ThermometerGroupConfig config = {};
  uint32_t lastRefreshMs = 0;
};

bool parseThermometerGroupConfig(const uint8_t *data,
                                 uint16_t dataSize,
                                 ThermometerGroupConfig *config);
bool serializeThermometerGroupConfig(const ThermometerGroupConfig &config,
                                     uint8_t *data,
                                     uint16_t dataSize,
                                     uint16_t *writtenSize = nullptr);

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_THERMOMETER_GROUP_H_
