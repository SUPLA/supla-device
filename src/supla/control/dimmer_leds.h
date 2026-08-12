// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_DIMMER_LEDS_H_
#define SRC_SUPLA_CONTROL_DIMMER_LEDS_H_

#include <supla/io.h>

#include "dimmer_base.h"

namespace Supla {
namespace Control {
class DimmerLeds : public DimmerBase {
 public:
  explicit DimmerLeds(Supla::Io::Base *io, int brightnessPin);
  explicit DimmerLeds(int brightnessPin);
  explicit DimmerLeds(Supla::Io::IoPin brightnessPin);

  void setRGBWValueOnDevice(uint32_t red,
                            uint32_t green,
                            uint32_t blue,
                            uint32_t brightness) override;

  void onInit() override;

 protected:
  Supla::Io::IoPin brightnessPin;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_DIMMER_LEDS_H_
