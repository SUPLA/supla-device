// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_H_

#include "general_purpose_channel_base.h"

namespace Supla {
namespace Sensor {
class GeneralPurposeMeasurement : public GeneralPurposeChannelBase {
 public:
  explicit GeneralPurposeMeasurement(MeasurementDriver *driver = nullptr,
      bool addMemoryVariableDriver = true);

 protected:
};

};  // namespace Sensor
};  // namespace Supla
#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_MEASUREMENT_H_
