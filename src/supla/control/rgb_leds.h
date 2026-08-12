// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_RGB_LEDS_H_
#define SRC_SUPLA_CONTROL_RGB_LEDS_H_

#include <supla/io.h>

#include "rgb_base.h"

namespace Supla {
namespace Control {
class RGBLeds : public RGBBase {
 public:
  RGBLeds(Supla::Io::Base *io, int redPin, int greenPin, int bluePin);
  RGBLeds(int redPin, int greenPin, int bluePin);
  RGBLeds(Supla::Io::IoPin redPin,
          Supla::Io::IoPin greenPin,
          Supla::Io::IoPin bluePin);

  void setRGBWValueOnDevice(uint32_t red,
                            uint32_t green,
                            uint32_t blue,
                            uint32_t brightness) override;

  void onInit() override;

 protected:
  Supla::Io::IoPin redPin;
  Supla::Io::IoPin greenPin;
  Supla::Io::IoPin bluePin;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGB_LEDS_H_
