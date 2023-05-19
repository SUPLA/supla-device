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

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/control/button.h>

#include "../actions.h"
#include "../io.h"
#include "../storage/storage.h"
#include "relay.h"

namespace Supla {
namespace Control {

Relay::Relay(Supla::Io *io,
             int pin,
             bool highIsOn,
             _supla_int_t functions)
    : Relay(pin, highIsOn, functions) {
  this->io = io;
}

Relay::Relay(int pin, bool highIsOn, _supla_int_t functions)
    : pin(pin),
      highIsOn(highIsOn),
      stateOnInit(STATE_ON_INIT_OFF),
      durationMs(0),
      storedTurnOnDurationMs(0),
      durationTimestamp(0),
      keepTurnOnDurationMs(false) {
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setFuncList(functions);
}

uint8_t Relay::pinOnValue() {
  return highIsOn ? HIGH : LOW;
}

uint8_t Relay::pinOffValue() {
  return highIsOn ? LOW : HIGH;
}

void Relay::onInit() {
  bool stateOn = false;
  if (stateOnInit == STATE_ON_INIT_ON ||
      stateOnInit == STATE_ON_INIT_RESTORED_ON) {
    stateOn = true;
  }

  if (attachedButton) {
    if (attachedButton->isMonostable()) {
      attachedButton->addAction(
          Supla::TOGGLE, this, Supla::CONDITIONAL_ON_PRESS);
    } else if (attachedButton->isBistable()) {
      attachedButton->addAction(
          Supla::TOGGLE, this, Supla::CONDITIONAL_ON_CHANGE);
    } else if (attachedButton->isMotionSensor()) {
      attachedButton->addAction(
          Supla::TURN_ON, this, Supla::ON_PRESS);
      attachedButton->addAction(
          Supla::TURN_OFF, this, Supla::ON_RELEASE);
      if (attachedButton->getLastState() == Supla::Control::PRESSED) {
        stateOn = true;
      } else {
        stateOn = false;
      }
    }
  }

  if (!isLastResetSoft()) {
    if (stateOn) {
      turnOn();
    } else {
      turnOff();
    }
  }

  // pin mode is set after setting pin value in order to
  // avoid problems with LOW trigger relays
  Supla::Io::pinMode(channel.getChannelNumber(), pin, OUTPUT, io);

  if (stateOn) {
    turnOn();
  } else {
    turnOff();
  }
}

void Relay::iterateAlways() {
  if (durationMs && millis() - durationTimestamp > durationMs) {
    toggle();
  }
}

int Relay::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  int result = -1;
  if (newValue->value[0] == 1) {
    if (keepTurnOnDurationMs) {
      storedTurnOnDurationMs = newValue->DurationMS;
    }
    turnOn(newValue->DurationMS);
    result = 1;
  } else if (newValue->value[0] == 0) {
    turnOff(0);  // newValue->DurationMS may contain "turn on duration" which
                 // result in unexpected "turn on after duration ms received in
                 // turnOff message"
    result = 1;
  }

  return result;
}

void Relay::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  if (keepTurnOnDurationMs) {
    value->DurationMS = storedTurnOnDurationMs;
  }
}

void Relay::turnOn(_supla_int_t duration) {
  SUPLA_LOG_INFO(
            "Relay[%d] turn ON (duration %d ms)",
            channel.getChannelNumber(),
            duration);
  durationMs = duration;
  durationTimestamp = millis();
  if (keepTurnOnDurationMs) {
    durationMs = storedTurnOnDurationMs;
  }
  Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOnValue(), io);

  channel.setNewValue(true);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000);
}

void Relay::turnOff(_supla_int_t duration) {
  SUPLA_LOG_INFO(
            "Relay[%d] turn OFF (duration %d ms)",
            channel.getChannelNumber(),
            duration);
  durationMs = duration;
  durationTimestamp = millis();
  Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOffValue(), io);

  channel.setNewValue(false);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(5000);
}

bool Relay::isOn() {
  return Supla::Io::digitalRead(channel.getChannelNumber(), pin, io) ==
         pinOnValue();
}

void Relay::toggle(_supla_int_t duration) {
  SUPLA_LOG_DEBUG(
            "Relay[%d] toggle (duration %d ms)",
            channel.getChannelNumber(),
            duration);
  if (isOn()) {
    turnOff(duration);
  } else {
    turnOn(duration);
  }
}

void Relay::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case TURN_ON: {
      turnOn();
      break;
    }
    case TURN_OFF: {
      turnOff();
      break;
    }
    case TOGGLE: {
      toggle();
      break;
    }
  }
}

void Relay::onSaveState() {
  Supla::Storage::WriteState((unsigned char *)&storedTurnOnDurationMs,
                             sizeof(storedTurnOnDurationMs));
  bool enabled = false;
  if (stateOnInit < 0) {
    enabled = isOn();
  }
  Supla::Storage::WriteState((unsigned char *)&enabled, sizeof(enabled));
}

void Relay::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&storedTurnOnDurationMs,
                            sizeof(storedTurnOnDurationMs));
  if (keepTurnOnDurationMs) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored durationMs: %d",
              channel.getChannelNumber(),
              storedTurnOnDurationMs);
  } else {
    storedTurnOnDurationMs = 0;
  }

  bool enabled = false;
  Supla::Storage::ReadState((unsigned char *)&enabled, sizeof(enabled));
  if (stateOnInit < 0) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored relay state: %s",
              channel.getChannelNumber(),
              enabled ? "ON" : "OFF");
    if (enabled) {
      stateOnInit = STATE_ON_INIT_RESTORED_ON;
    } else {
      stateOnInit = STATE_ON_INIT_RESTORED_OFF;
    }
  }
}

Relay &Relay::setDefaultStateOn() {
  stateOnInit = STATE_ON_INIT_ON;
  return *this;
}

Relay &Relay::setDefaultStateOff() {
  stateOnInit = STATE_ON_INIT_OFF;
  return *this;
}

Relay &Relay::setDefaultStateRestore() {
  stateOnInit = STATE_ON_INIT_RESTORE;
  return *this;
}

Relay &Relay::keepTurnOnDuration(bool keep) {
  keepTurnOnDurationMs = keep;
  return *this;
}

unsigned _supla_int_t Relay::getStoredTurnOnDurationMs() {
  return storedTurnOnDurationMs;
}

void Relay::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

}  // namespace Control
}  // namespace Supla
