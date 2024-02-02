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

#ifndef SRC_SUPLA_CONTROL_SIMPLE_BUTTON_H_
#define SRC_SUPLA_CONTROL_SIMPLE_BUTTON_H_

#include <stdint.h>

#include "../element.h"
#include "../events.h"
#include "../local_action.h"

namespace Supla {

class Io;

namespace Control {

enum StateResults { PRESSED, RELEASED, TO_PRESSED, TO_RELEASED };

class ButtonState {
 public:
  ButtonState(Supla::Io *io, int pin, bool pullUp, bool invertLogic);
  ButtonState(int pin, bool pullUp, bool invertLogic);
  enum StateResults update();
  enum StateResults getLastState() const;
  void init(int buttonNumber);

  void setSwNoiseFilterDelay(unsigned int newDelayMs);
  void setDebounceDelay(unsigned int newDelayMs);
  int getGpio() const;

 protected:
  int valueOnPress() const;

  Supla::Io *io = nullptr;

  uint16_t debounceDelayMs = 50;
  uint16_t swNoiseFilterDelayMs = 20;
  uint32_t debounceTimestampMs = 0;
  uint32_t filterTimestampMs = 0;
  int16_t pin = -1;
  int8_t newStatusCandidate = 0;
  int8_t prevState = -1;
  bool pullUp = false;
  bool invertLogic = false;
};

class SimpleButton : public Element, public LocalAction {
 public:
  explicit SimpleButton(Supla::Io *io,
                        int pin,
                        bool pullUp = false,
                        bool invertLogic = false);
  explicit SimpleButton(int pin, bool pullUp = false, bool invertLogic = false);

  void onTimer() override;
  void onInit() override;
  void setSwNoiseFilterDelay(unsigned int newDelayMs);
  void setDebounceDelay(unsigned int newDelayMs);

  enum StateResults getLastState() const;

 protected:
  // Returns unique button number (current implementation returns configured
  // GPIO)
  virtual int8_t getButtonNumber() const;
  ButtonState state;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_SIMPLE_BUTTON_H_
