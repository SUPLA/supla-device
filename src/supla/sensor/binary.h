// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_BINARY_H_
#define SRC_SUPLA_SENSOR_BINARY_H_

#include <supla/io.h>
#include <supla/sensor/binary_base.h>

namespace Supla {

namespace Io {
}

namespace Sensor {
class Binary : public BinaryBase {
 public:
  explicit Binary(Supla::Io::IoPin inputPin);
  explicit Binary(Supla::Io::Base *io,
                  int pin,
                  bool pullUp = false,
                  bool invertLogic = false);
  explicit Binary(int pin, bool pullUp = false, bool invertLogic = false);
  bool getValue() override;
  void onInit() override;

 protected:
  Supla::Io::IoPin inputPin;
  bool newStateCandidateValue = false;
  bool prevValue = false;
  uint32_t lastStateChangeMs = 0;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BINARY_H_
