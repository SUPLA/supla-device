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

#ifndef SRC_SUPLA_CONTROL_BUTTON_H_
#define SRC_SUPLA_CONTROL_BUTTON_H_

#include <stdint.h>
#include "simple_button.h"

namespace Supla {
namespace Control {

class Button : public SimpleButton {
 public:
  explicit Button(int pin, bool pullUp = false, bool invertLogic = false);

  void onTimer() override;
  void onLoadConfig() override;
  void setHoldTime(unsigned int timeMs);
  void repeatOnHoldEvery(unsigned int timeMs);
  void setMulticlickTime(unsigned int timeMs, bool bistableButton = false);
  bool isBistable() const;

 protected:
  unsigned int holdTimeMs;
  unsigned int repeatOnHoldMs;
  unsigned int multiclickTimeMs;
  uint64_t lastStateChangeMs;
  uint8_t clickCounter;
  unsigned int holdSend;
  bool bistable;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_BUTTON_H_
