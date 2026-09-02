// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

// Dependency: Please install "BH1750" by Christopher Laws library in Arduino:
// https://github.com/claws/BH1750

#ifndef SRC_SUPLA_SENSOR_BH1750_H_
#define SRC_SUPLA_SENSOR_BH1750_H_

#include <supla/sensor/general_purpose_measurement.h>

#include <math.h>
#include <BH1750.h>
#include <supla/log_wrapper.h>

namespace Supla {
namespace Sensor {
class Bh1750 : public GeneralPurposeMeasurement {
 public:
  explicit Bh1750(int i2cAddress = 0x23)
      : GeneralPurposeMeasurement(nullptr, false), sensor(i2cAddress) {
    setDefaultUnitAfterValue("lx");
    getChannel()->setDefaultIcon(14);
  }

  void initializeSensor() {
    if (sensor.begin()) {
      SUPLA_LOG_DEBUG("BH1750[%d]: initialized", getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("BH1750[%d]: not found or other error",
                        getChannelNumber());
    }
  }

  void onInit() override {
    initializeSensor();

    channel.setNewValue(getValue());
  }

 protected:
  double getValue() override {
    double value = sensor.readLightLevel();
    if (isnan(value) || value < 0) {
      invalidCounter++;
      if (invalidCounter < 4) {
        return lastValue;
      } else {
        // try to init again
        invalidCounter = 0;
        initializeSensor();
      }
      lastValue = NAN;
      return lastValue;
    }
    invalidCounter = 0;
    lastValue = value;
    return value;
  }

  BH1750 sensor;

  double lastValue = NAN;
  int invalidCounter = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BH1750_H_
