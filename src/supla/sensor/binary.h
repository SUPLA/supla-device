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

#ifndef SRC_SUPLA_SENSOR_BINARY_H_
#define SRC_SUPLA_SENSOR_BINARY_H_

#include "../channel_element.h"

namespace Supla {

class Io;

namespace Sensor {
class Binary : public ChannelElement {
 public:
  explicit Binary(Supla::Io *io,
                  int pin,
                  bool pullUp = false,
                  bool invertLogic = false);
  explicit Binary(int pin, bool pullUp = false, bool invertLogic = false);
  bool getValue();
  void iterateAlways() override;
  void onInit() override;

 protected:
  int pin = -1;
  bool pullUp = false;
  bool invertLogic = false;
  uint32_t lastReadTime = 0;
  Supla::Io *io = nullptr;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_BINARY_H_
