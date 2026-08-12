// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_NORMALLY_OPEN_H_
#define SRC_SUPLA_SENSOR_NORMALLY_OPEN_H_

#include "binary.h"

namespace Supla {
namespace Sensor {
class NormallyOpen : public Binary {
 public:
  explicit NormallyOpen(int pin, bool pullUp = false, bool invertLogic = false)
      : Binary(pin, pullUp, invertLogic) {
  }
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_NORMALLY_OPEN_H_
