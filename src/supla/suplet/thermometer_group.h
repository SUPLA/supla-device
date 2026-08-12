// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
