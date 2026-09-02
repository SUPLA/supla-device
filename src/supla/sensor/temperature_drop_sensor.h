// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_
#define SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_

#include <supla/element.h>

#include "virtual_binary.h"
#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {

#define MAX_TEMPERATURE_MEASUREMENTS 60

class TemperatureDropSensor : public Supla::Element {
 public:
  explicit TemperatureDropSensor(ThermHygroMeter *thermometer);

  void onInit() override;
  void iterateAlways() override;

  bool isDropDetected() const;
  int getBinarySensorChannelNo() const;

  /**
   * Set temperature drop threshold detection
   *
   * @param threshold in 0.01 units, so -200 is -2 degrees
   */
  void setTemperatureDropThreshold(int16_t threshold);

  /**
   * Set drop detection delay. When temperature drops below threshold, drop
   * detection is delayed for this amount of time.
   *
   * @param delayMs in milliseconds, use 0 to disable
   */
  void setDropDetectionDelayMs(uint32_t delayMs);

 private:
  Supla::Sensor::VirtualBinary virtualBinary;
  ThermHygroMeter *thermometer = nullptr;

  int16_t getAverage(int fromIndex, int toIndex) const;
  bool detectTemperatureDrop(int16_t temperature, int16_t *average) const;

  uint32_t lastTemperatureUpdate = 0;
  uint32_t filteringTimestamp = 0;
  uint32_t dropDetectionTimestamp = 0;
  uint32_t probeIntervalMs = 30000;
  uint32_t dropDetectionDelayMs = 0;

  int16_t measurements[MAX_TEMPERATURE_MEASUREMENTS] = {};
  int measurementIndex = 0;
  int16_t temperatureDropThreshold = -200;  // -2 degree
  int16_t averageAtDropDetection = INT16_MIN;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_TEMPERATURE_DROP_SENSOR_H_
