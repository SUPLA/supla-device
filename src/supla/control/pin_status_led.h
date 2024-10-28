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

#ifndef SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_
#define SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_

#include "../element.h"

namespace Supla {

class Io;

namespace Control {
class PinStatusLed : public Element {
 public:
  PinStatusLed(Supla::Io *ioSrc,
               Supla::Io *ioOut,
               uint8_t srcPin,
               uint8_t outPin,
               bool invert = false);
  PinStatusLed(uint8_t srcPin, uint8_t outPin, bool invert = false);

  void onInit() override;
  void iterateAlways() override;
  void onTimer() override;

  void setInvertedLogic(bool invertedLogic);
  void setWorkOnTimer(bool workOnTimer);

 protected:
  void updatePin();

  uint8_t srcPin = 0;
  uint8_t outPin = 0;
  bool invert = false;
  bool workOnTimer = false;
  Supla::Io *ioSrc = nullptr;
  Supla::Io *ioOut = nullptr;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_PIN_STATUS_LED_H_
