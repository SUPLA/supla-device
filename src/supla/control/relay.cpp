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

/*
 * TODO(klew):
 *  - add sending timer value to server
 */

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/control/button.h>
#include <supla/protocol/protocol_layer.h>

#include "../actions.h"
#include "../io.h"
#include "../storage/storage.h"
#include "relay.h"

using Supla::Control::Relay;

Relay::Relay(Supla::Io *io,
             int pin,
             bool highIsOn,
             _supla_int_t functions)
    : Relay(pin, highIsOn, functions) {
  this->io = io;
}

Relay::Relay(int pin, bool highIsOn, _supla_int_t functions)
    : pin(pin),
      highIsOn(highIsOn) {
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  channel.setFuncList(functions);
}

void Relay::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::Element::onRegistered(suplaSrpc);
  channel.requestChannelConfig();

  timerUpdateTimestamp = 1;
}

uint8_t Relay::handleChannelConfig(TSD_ChannelConfig *result,
                                   bool local) {
  (void)(local);
  SUPLA_LOG_DEBUG(
      "Relay::handleChannelConfig, func %d, configtype %d, configsize %d",
      result->Func,
      result->ConfigType,
      result->ConfigSize);
  setChannelFunction(result->Func);
  switch (result->Func) {
    default:
    case SUPLA_CHANNELFNC_LIGHTSWITCH:
    case SUPLA_CHANNELFNC_POWERSWITCH: {
      break;
    }

    case SUPLA_CHANNELFNC_STAIRCASETIMER: {
      if (result->ConfigType == 0 &&
          result->ConfigSize == sizeof(TChannelConfig_StaircaseTimer)) {
        uint32_t newDurationMs =
            reinterpret_cast<TChannelConfig_StaircaseTimer *>(result->Config)
                ->TimeMS;
        if (newDurationMs != storedTurnOnDurationMs) {
          storedTurnOnDurationMs = newDurationMs;
          Supla::Storage::ScheduleSave(2000);
        }
      }
      break;
    }

    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK: {
      // add here reading of duration from config when it will be added
      break;
    }
  }
  return SUPLA_CONFIG_RESULT_TRUE;
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
    attachedButton->onInit();  // make sure button was initialized
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

  uint32_t duration = durationMs;
  if (!isLastResetSoft()) {
    if (stateOn) {
      turnOn(duration);
    } else {
      turnOff(duration);
    }
  }

  // pin mode is set after setting pin value in order to
  // avoid problems with LOW trigger relays
  Supla::Io::pinMode(channel.getChannelNumber(), pin, OUTPUT, io);

  if (stateOn) {
    turnOn(duration);
  } else {
    turnOff(duration);
  }
}

void Relay::iterateAlways() {
  if (durationMs && millis() - durationTimestamp > durationMs) {
    toggle();
  }
}

bool Relay::iterateConnected() {
  if (timerUpdateTimestamp != durationTimestamp) {
    timerUpdateTimestamp = durationTimestamp;
    updateTimerValue();
  }

  return ChannelElement::iterateConnected();
}

int Relay::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  int result = -1;
  if (newValue->value[0] == 1) {
    if (isImpulseFunction()) {
      storedTurnOnDurationMs = newValue->DurationMS;
    }

    uint32_t copyDurationMs = storedTurnOnDurationMs;
    if (isStaircaseFunction() && newValue->DurationMS == 0) {
      // in case of staircase timer we allow duration == 0 from server, which
      // means that this time we should turn on without automatic turn off.
      storedTurnOnDurationMs = 0;
    }

    turnOn(newValue->DurationMS);
    storedTurnOnDurationMs = copyDurationMs;
    result = 1;
  } else if (newValue->value[0] == 0) {
    if (keepTurnOnDurationMs || isStaircaseFunction() || isImpulseFunction()) {
    turnOff(0);  // newValue->DurationMS may contain "turn on duration" which
                 // result in unexpected "turn on after duration ms received in
                 // turnOff message"
    } else {
      turnOff(newValue->DurationMS);
    }
    result = 1;
  }

  return result;
}

void Relay::fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }

  if (keepTurnOnDurationMs || isStaircaseFunction() || isImpulseFunction()) {
    value->DurationMS = storedTurnOnDurationMs;
  }
}

void Relay::turnOn(_supla_int_t duration) {
  SUPLA_LOG_INFO(
            "Relay[%d] turn ON (duration %d ms)",
            channel.getChannelNumber(),
            duration);
  durationMs = duration;
  if (keepTurnOnDurationMs || isStaircaseFunction() || isImpulseFunction()) {
    durationMs = storedTurnOnDurationMs;
  }
  if (durationMs != 0) {
    durationTimestamp = millis();
  } else {
    durationTimestamp = 0;
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
  if (durationMs != 0) {
    durationTimestamp = millis();
  } else {
    durationTimestamp = 0;
  }
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
    case TURN_ON_WITHOUT_TIMER: {
      uint32_t copyDurationMs = storedTurnOnDurationMs;
      storedTurnOnDurationMs = 0;
      SUPLA_LOG_DEBUG("Relay[%d]: override stored durationMs",
                      channel.getChannelNumber());
      turnOn();
      storedTurnOnDurationMs = copyDurationMs;
      break;
    }
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
  uint32_t durationForState = storedTurnOnDurationMs;
  uint8_t relayFlags = 0;
  if (isStaircaseFunction()) {
    relayFlags |= RELAY_FLAGS_STAIRCASE;
  } else if (isImpulseFunction()) {
    relayFlags |= RELAY_FLAGS_IMPULSE_FUNCTION;
  } else if (isCountdownTimerFunctionEnabled() && stateOnInit < 0) {
    // for other functions we store remaining countdown timer value
    durationForState = 0;
    if (durationMs) {
      uint32_t elapsedTimeMs = millis() - durationTimestamp;
      if (elapsedTimeMs < durationMs) {
        // remaining time should always be lower than durationMs in other cases
        // it means that timer already expired and it will be toggled on next
        // iterateAlways()
        durationForState = durationMs - elapsedTimeMs;
      }
    }
  }

  Supla::Storage::WriteState(
      reinterpret_cast<unsigned char *>(&durationForState),
      sizeof(durationForState));
  if (stateOnInit < 0) {
    relayFlags |= (isOn() ? RELAY_FLAGS_ON : 0);
  }

  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&relayFlags),
                             sizeof(relayFlags));
}

void Relay::onLoadState() {
  Supla::Storage::ReadState(
      reinterpret_cast<unsigned char *>(&storedTurnOnDurationMs),
      sizeof(storedTurnOnDurationMs));
  uint8_t relayFlags = 0;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&relayFlags),
                            sizeof(relayFlags));
  if (stateOnInit < 0) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored relay state: %s",
              channel.getChannelNumber(),
              (relayFlags & RELAY_FLAGS_ON) ? "ON" : "OFF");
    if (relayFlags & RELAY_FLAGS_ON) {
      stateOnInit = STATE_ON_INIT_RESTORED_ON;
    } else {
      stateOnInit = STATE_ON_INIT_RESTORED_OFF;
    }
  }
  if (relayFlags & RELAY_FLAGS_STAIRCASE) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored staircase function",
              channel.getChannelNumber());
    setChannelFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
  } else if (relayFlags & RELAY_FLAGS_IMPULSE_FUNCTION) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored impulse function",
              channel.getChannelNumber());
    // actual funciton may be different, but we only have 8 bit bitfiled to
    // keep the state and currently it doesn't matter which "impulse function"
    // is actually used, so we set it to "controlling the gate"
    setChannelFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
  }
  if (isStaircaseFunction() || isImpulseFunction()) {
    SUPLA_LOG_INFO(
              "Relay[%d]: restored durationMs: %d",
              channel.getChannelNumber(),
              storedTurnOnDurationMs);
  } else {
    // restore remaining countdown timer value
    // relay will be on/off in onInit() with configured durationMs
    if (stateOnInit < 0) {
      durationMs = storedTurnOnDurationMs;
    }
    storedTurnOnDurationMs = 0;
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
  (void)(keep);
  // empty method left for compatibility
  // DEPREACATED
  return *this;
}

unsigned _supla_int_t Relay::getStoredTurnOnDurationMs() {
  return storedTurnOnDurationMs;
}

void Relay::attach(Supla::Control::Button *button) {
  attachedButton = button;
}


bool Relay::isStaircaseFunction() const {
  return channelFunction == SUPLA_CHANNELFNC_STAIRCASETIMER;
}

bool Relay::isImpulseFunction() const {
  return (channelFunction == SUPLA_CHANNELFNC_CONTROLLINGTHEGATE ||
          channelFunction == SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK ||
          channelFunction == SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK ||
          channelFunction == SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR);
}

void Relay::setChannelFunction(_supla_int_t newFunction) {
  bool wasImpulseFunction = isImpulseFunction();
  bool wasStaircaseFunction = isStaircaseFunction();
  channelFunction = newFunction;
  if (wasImpulseFunction != isImpulseFunction()) {
    Supla::Storage::ScheduleSave(2000);
  }
  if (wasStaircaseFunction != isStaircaseFunction()) {
    Supla::Storage::ScheduleSave(2000);
  }

  if (isStaircaseFunction() || isImpulseFunction()) {
    keepTurnOnDurationMs = true;
  } else {
    keepTurnOnDurationMs = false;
    storedTurnOnDurationMs = 0;
  }
}

void Relay::updateTimerValue() {
  uint32_t remainingTime = 0;
  uint8_t state = 0;
  int32_t senderId = 0;

  if (durationMs != 0) {
    uint32_t elapsedTimeMs = millis() - durationTimestamp;
    if (elapsedTimeMs <= durationMs) {
      remainingTime = durationMs - elapsedTimeMs;
      state = (isOn() ? 0 : 1);
    }
  }

  SUPLA_LOG_DEBUG("Relay[%d]: updating timer value: remainingTime=%d state=%d",
                  channel.getChannelNumber(),
                  remainingTime,
                  state);

  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->sendRemainingTimeValue(
        getChannelNumber(), remainingTime, state, senderId);
  }
}

void Relay::disableCountdownTimerFunction() {
  channel.unsetFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
}

void Relay::enableCountdownTimerFunction() {
  channel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
}

bool Relay::isCountdownTimerFunctionEnabled() const {
  return channel.getFlags() & SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED;
}
