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

#ifndef SRC_SUPLA_CONTROL_RGB_LEDS_H_
#define SRC_SUPLA_CONTROL_RGB_LEDS_H_

#include "rgb_base.h"

namespace Supla {
class Io;
namespace Control {
class RGBLeds : public RGBBase {
 public:
  RGBLeds(Supla::Io *io, int redPin, int greenPin, int bluePin);
  RGBLeds(int redPin, int greenPin, int bluePin);

  void setRGBWValueOnDevice(uint32_t red,
                            uint32_t green,
                            uint32_t blue,
                            uint32_t colorBrightness,
                            uint32_t brightness) override;

  void onInit() override;

 protected:
  Supla::Io *io = nullptr;
  int redPin = -1;
  int greenPin = -1;
  int bluePin = -1;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RGB_LEDS_H_
