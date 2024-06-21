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

#ifndef SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_
#define SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_

#include "../action_handler.h"
#include "../element.h"
#include "../local_action.h"
#include "output_interface.h"

#define STATE_ON_INIT_OFF 0
#define STATE_ON_INIT_ON  1

namespace Supla {

class Io;

namespace Control {
class InternalPinOutput : public Element,
                          public ActionHandler,
                          public LocalAction,
                          public OutputInterface {
 public:
  explicit InternalPinOutput(Supla::Io *io, int pin, bool highIsOn = true);
  explicit InternalPinOutput(int pin, bool highIsOn = true);

  virtual InternalPinOutput &setDefaultStateOn();
  virtual InternalPinOutput &setDefaultStateOff();
  virtual InternalPinOutput &setDurationMs(_supla_int_t duration);

  virtual uint8_t pinOnValue();
  virtual uint8_t pinOffValue();
  virtual void turnOn(_supla_int_t duration = 0);
  virtual void turnOff(_supla_int_t duration = 0);
  virtual bool isOn();
  virtual void toggle(_supla_int_t duration = 0);

  void handleAction(int event, int action) override;

  void onInit() override;
  void iterateAlways() override;

  int getOutputValue() const override;
  void setOutputValue(int value) override;
  bool isOnOffOnly() const override;

 protected:
  int pin = -1;
  bool highIsOn = true;
  int8_t stateOnInit = STATE_ON_INIT_OFF;
  unsigned _supla_int_t durationMs = 0;
  unsigned _supla_int_t storedTurnOnDurationMs = 0;
  uint32_t durationTimestamp = 0;
  int lastOutputValue = 0;
  Supla::Io *io = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_INTERNAL_PIN_OUTPUT_H_
