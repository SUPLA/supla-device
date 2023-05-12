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

/* Relay class
 * This class is used to control any type of relay that can be controlled
 * by setting LOW or HIGH output on selected GPIO.
 */

#ifndef SRC_SUPLA_CONTROL_RELAY_H_
#define SRC_SUPLA_CONTROL_RELAY_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../actions.h"
#include "../channel_element.h"
#include "../io.h"
#include "../local_action.h"
#include "../storage/storage.h"

#define STATE_ON_INIT_RESTORED_OFF -3
#define STATE_ON_INIT_RESTORED_ON  -2
#define STATE_ON_INIT_RESTORE      -1
#define STATE_ON_INIT_OFF          0
#define STATE_ON_INIT_ON           1

namespace Supla {
namespace Control {
class Button;

class Relay : public ChannelElement, public ActionHandler {
 public:
  explicit Relay(Supla::Io *io, int pin,
        bool highIsOn = true,
        _supla_int_t functions = (0xFF ^
                                  SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  explicit Relay(int pin,
        bool highIsOn = true,
        _supla_int_t functions = (0xFF ^
                                  SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  virtual Relay &setDefaultStateOn();
  virtual Relay &setDefaultStateOff();
  virtual Relay &setDefaultStateRestore();
  virtual Relay &keepTurnOnDuration(bool keep = true);

  virtual uint8_t pinOnValue();
  virtual uint8_t pinOffValue();
  virtual void turnOn(_supla_int_t duration = 0);
  virtual void turnOff(_supla_int_t duration = 0);
  virtual bool isOn();
  virtual void toggle(_supla_int_t duration = 0);

  void attach(Supla::Control::Button *);

  void handleAction(int event, int action) override;

  void onInit() override;
  void onLoadState() override;
  void onSaveState() override;
  void iterateAlways() override;
  int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  unsigned _supla_int_t getStoredTurnOnDurationMs();

 protected:
  int pin;
  bool highIsOn;

  int8_t stateOnInit;

  unsigned _supla_int_t durationMs;
  unsigned _supla_int_t storedTurnOnDurationMs;
  uint64_t durationTimestamp;
  bool keepTurnOnDurationMs;

  Supla::Io *io = nullptr;
  Supla::Control::Button *attachedButton = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_H_
