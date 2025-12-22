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

#include "relay.h"

#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/control/button.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/control/relay_hvac_aggregator.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/condition.h>
#include <supla/condition_getter.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/io.h>


using Supla::Control::Relay;

int16_t Relay::relayStorageSaveDelay = 5000;

void Relay::setRelayStorageSaveDelay(int delayMs) {
  relayStorageSaveDelay = delayMs;
}

Relay::Relay(Supla::Io::Base *io,
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
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFuncList(functions);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

Relay::~Relay() {
  ButtonListElement* currentElement = buttonList;
  while (currentElement) {
    ButtonListElement* nextElement = currentElement->next;
    delete currentElement;
    currentElement = nextElement;
  }
  Supla::Control::RelayHvacAggregator::Remove(getChannelNumber());
}

void Relay::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();
    loadConfigChangeFlag();
    updateRelayHvacAggregator();

    if (overcurrentMaxAllowed > 0) {
      uint32_t overcurrentValue = 0;
      char key[16] = {};
      generateKey(key, Supla::ConfigTag::RelayOvercurrentThreshold);
      cfg->getUInt32(key, &overcurrentValue);
      if (overcurrentValue > overcurrentMaxAllowed) {
        overcurrentValue = overcurrentMaxAllowed;
      }
      overcurrentThreshold = overcurrentValue;
      SUPLA_LOG_DEBUG("Relay[%d] overcurrent threshold: %d (max: %d)",
                      getChannelNumber(),
                      overcurrentThreshold,
                      overcurrentMaxAllowed);
    }
  }
  if (isStaircaseFunction() || isImpulseFunction()) {
    if (storedTurnOnDurationMs == 0) {
      storedTurnOnDurationMs =
          (isStaircaseFunction() ? defaultStaircaseDurationMs
                                 : defaultImpulseDurationMs);
    }
    if (isStaircaseFunction()) {
      usedConfigTypes.set(SUPLA_CONFIG_TYPE_EXTENDED);
    }
  }
}

void Relay::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::ElementWithChannelActions::onRegistered(suplaSrpc);

  timerUpdateTimestamp = 1;
}

Supla::ApplyConfigResult Relay::applyChannelConfig(TSD_ChannelConfig *result,
                                                   bool) {
  SUPLA_LOG_DEBUG(
      "Relay[%d] applyChannelConfig, func %d, configtype %d, configsize %d",
      getChannelNumber(),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  updateRelayHvacAggregator();

  if (result->ConfigSize == 0) {
    if (isImpulseFunction()) {
      // In case of impulse function we can ignore empty config - it is not
      // implemented on server side either.
      return Supla::ApplyConfigResult::Success;
    }
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  bool readonlyViolation = false;
  if (((result->Func == SUPLA_CHANNELFNC_LIGHTSWITCH ||
        result->Func == SUPLA_CHANNELFNC_POWERSWITCH) &&
       result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT) ||
      (result->Func == SUPLA_CHANNELFNC_STAIRCASETIMER &&
       result->ConfigType == SUPLA_CONFIG_TYPE_EXTENDED)) {
    if (result->ConfigSize == sizeof(TChannelConfig_PowerSwitch)) {
      auto config =
          reinterpret_cast<TChannelConfig_PowerSwitch *>(result->Config);
      SUPLA_LOG_DEBUG(
          "Relay[%d] OvercurrentMaxAllowed: %d, OvercurrentThreshold: %d",
          getChannelNumber(),
          config->OvercurrentMaxAllowed,
          config->OvercurrentThreshold);

      if (config->OvercurrentMaxAllowed != overcurrentMaxAllowed) {
        SUPLA_LOG_INFO(
            "Relay[%d] OvercurrentMaxAllowed on server is not valid (%d), "
            "setting to %d",
            getChannelNumber(),
            config->OvercurrentMaxAllowed,
            overcurrentMaxAllowed);
        readonlyViolation = true;
      }
      if (defaultRelatedMeterChannelNo >= 0) {
        if (config->DefaultRelatedMeterIsSet == 0 ||
            config->DefaultRelatedMeterChannelNo !=
                defaultRelatedMeterChannelNo) {
          SUPLA_LOG_INFO(
              "Relay[%d] DefaultRelatedMeterChannelNo on server is not valid "
              "(%d), setting to %d",
              getChannelNumber(),
              config->DefaultRelatedMeterChannelNo,
              defaultRelatedMeterChannelNo);
          readonlyViolation = true;
        }
      }
      if (defaultRelatedMeterChannelNo == -1 &&
          config->DefaultRelatedMeterIsSet == 1) {
        SUPLA_LOG_INFO(
            "Relay[%d] DefaultRelatedMeterIsSet on server is not valid "
            "(%d), setting to 0",
            getChannelNumber(),
            config->DefaultRelatedMeterIsSet);
        readonlyViolation = true;
      }
      if (config->OvercurrentThreshold != overcurrentThreshold) {
        SUPLA_LOG_DEBUG("Relay[%d] OvercurrentThreshold changed from %d to %d",
                        getChannelNumber(),
                        overcurrentThreshold,
                        config->OvercurrentThreshold);
        setOvercurrentThreshold(config->OvercurrentThreshold);
        overcurrentActiveTimestamp = 0;
      }
    }
  } else if (result->Func == SUPLA_CHANNELFNC_STAIRCASETIMER) {
    if (result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
        result->ConfigSize == sizeof(TChannelConfig_StaircaseTimer)) {
      uint32_t newDurationMs =
          reinterpret_cast<TChannelConfig_StaircaseTimer *>(result->Config)
              ->TimeMS;
      if (newDurationMs == 0 && storedTurnOnDurationMs != 0) {
        SUPLA_LOG_DEBUG("Relay[%d] missing StaircaseTimer duration, setting to "
                        "%d",
                      getChannelNumber(),
                      storedTurnOnDurationMs);
        return Supla::ApplyConfigResult::SetChannelConfigNeeded;
      }
      if (newDurationMs != storedTurnOnDurationMs) {
        SUPLA_LOG_DEBUG("Relay[%d] new StaircaseTimer duration %d",
                        getChannelNumber(),
                        newDurationMs);
        storedTurnOnDurationMs = newDurationMs;
        Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
      }
    }
  } else if (result->Func == SUPLA_CHANNELFNC_CONTROLLINGTHEGATE ||
             result->Func == SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK ||
             result->Func == SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR ||
             result->Func == SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK) {
    SUPLA_LOG_DEBUG("Relay[%d] Ignoring config for controlling the gate/door",
                    getChannelNumber());
    // TODO(klew): add here reading of duration from config when it will be
    // added
  }

  return (readonlyViolation ? Supla::ApplyConfigResult::SetChannelConfigNeeded
                            : Supla::ApplyConfigResult::Success);
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

  if (skipInitialStateSetting) {
    skipInitialStateSetting = false;
    for (auto buttonListElement = buttonList; buttonListElement;
         buttonListElement = buttonListElement->next) {
      auto attachedButton = buttonListElement->button;
      if (attachedButton) {
        if (attachedButton->isMotionSensor() ||
                   attachedButton->isCentral()) {
          if (attachedButton->isReady()) {
            if (attachedButton->getLastState() == Supla::Control::PRESSED) {
              stateOn = true;
            } else {
              stateOn = false;
            }
          } else {
            skipInitialStateSetting = true;
            return;
          }
        }
      }
    }
  } else {
    for (auto buttonListElement = buttonList; buttonListElement;
         buttonListElement = buttonListElement->next) {
      auto attachedButton = buttonListElement->button;
      if (attachedButton) {
        attachedButton->onInit();  // make sure button was initialized
        if (attachedButton->isMonostable()) {
          attachedButton->addAction(
              Supla::TOGGLE, this, Supla::CONDITIONAL_ON_PRESS);
        } else if (attachedButton->isBistable()) {
          attachedButton->addAction(Supla::TOGGLE_WITH_POSTPONED_COMM,
                                    this,
                                    Supla::CONDITIONAL_ON_CHANGE);
        } else if (attachedButton->isMotionSensor() ||
                   attachedButton->isCentral()) {
          attachedButton->addAction(Supla::TURN_ON, this, Supla::ON_PRESS);
          attachedButton->addAction(Supla::TURN_OFF, this, Supla::ON_RELEASE);
          if (!attachedButton->isReady()) {
            skipInitialStateSetting = true;
          } else {
            if (attachedButton->getLastState() == Supla::Control::PRESSED) {
              stateOn = true;
            } else {
              stateOn = false;
            }
          }
        }
      }
    }
  }
  initDone = true;

  if (!skipInitialStateSetting) {
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
    if (pin >= 0) {
      Supla::Io::pinMode(channel.getChannelNumber(), pin, OUTPUT, io);
    }

    if (stateOn) {
      turnOn(duration);
    } else {
      turnOff(duration);
    }
    SUPLA_LOG_DEBUG("Relay[%d] init done, storedTurnOnDurationMs %d",
                    channel.getChannelNumber(),
                    storedTurnOnDurationMs);
  } else {
    SUPLA_LOG_DEBUG("Relay[%d] init skipped, button state not ready",
                    channel.getChannelNumber());
  }
}

void Relay::iterateAlways() {
  if (!isFullyInitialized()) {
    onInit();
    if (!isFullyInitialized()) {
      return;
    }
  }

  if (durationMs && millis() - durationTimestamp > durationMs) {
    toggle();
  }

  if (overcurrentThreshold > 0 && isOn()) {
    if (millis() - overcurrentCheckTimestamp > 500) {
      overcurrentCheckTimestamp = millis();
      auto current = getCurrentValueFromMeter();
      if (current > overcurrentThreshold * 1.2) {
        SUPLA_LOG_WARNING(
            "Relay[%d] Overcurrent detected (%d) - instant turn off",
            getChannelNumber(), current);
        channel.setRelayOvercurrentCutOff(true);
        turnOff();
        return;
      }
      if (current >= overcurrentThreshold) {
        if (overcurrentActiveTimestamp != 0 &&
            millis() - overcurrentActiveTimestamp > 30 * 1000) {  // 30 s
          SUPLA_LOG_WARNING(
              "Relay[%d] Overcurrent detected (%d) - turn off",
              getChannelNumber(),
              current);
          channel.setRelayOvercurrentCutOff(true);
          turnOff();
          return;
        }
        if (overcurrentActiveTimestamp == 0) {
          SUPLA_LOG_WARNING(
              "Relay[%d] Overcurrent detected (%d) - filtering 30s",
              getChannelNumber(),
              current);
          overcurrentActiveTimestamp = millis();
        }
      } else {
        if (overcurrentActiveTimestamp != 0) {
          SUPLA_LOG_DEBUG(
              "Relay[%d] Overcurrent filtering cancelled (%d)",
              getChannelNumber(),
              current);
        }
        overcurrentActiveTimestamp = 0;
      }
    }
  } else {
    overcurrentCheckTimestamp = 0;
    overcurrentActiveTimestamp = 0;
  }
}

bool Relay::iterateConnected() {
  if (postponeCommTimestamp != 0 &&  millis() - postponeCommTimestamp < 500) {
    return true;
  }
  postponeCommTimestamp = 0;

  if (timerUpdateTimestamp != durationTimestamp) {
    timerUpdateTimestamp = durationTimestamp;
    updateTimerValue();
    return false;
  }

  return ChannelElement::iterateConnected();
}

int32_t Relay::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  auto channelFunction = getChannel()->getDefaultFunction();
  bool zeroDurationAllowed = false;
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_PUMPSWITCH:
    case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH: {
      SUPLA_LOG_WARNING("Relay[%d] ignoring server request (pump/heatorcold)",
                        getChannelNumber());
      return 0;
    }
    case SUPLA_CHANNELFNC_POWERSWITCH:
    case SUPLA_CHANNELFNC_LIGHTSWITCH: {
      zeroDurationAllowed = true;
      break;
    }
    default: {}
  }

  int result = -1;
  if (newValue->value[0] == 1) {
    if (!zeroDurationAllowed &&
        newValue->DurationMS < minimumAllowedDurationMs) {
      SUPLA_LOG_DEBUG("Relay[%d] override duration with min value",
                      channel.getChannelNumber());
      newValue->DurationMS = minimumAllowedDurationMs;
    }
    if ((isImpulseFunction() || isCyclicMode()) && newValue->DurationMS > 0) {
      storedTurnOnDurationMs = newValue->DurationMS;
    }

    uint32_t copyDurationMs = storedTurnOnDurationMs;
    if (isStaircaseFunction() && newValue->DurationMS == 0) {
      // in case of staircase timer we allow duration == 0 from server, which
      // means that this time we should turn on without automatic turn off.
      storedTurnOnDurationMs = 0;
    }

    turnOn(isCyclicMode() ? storedTurnOnDurationMs : newValue->DurationMS);
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
  if (!isFullyInitialized()) {
    SUPLA_LOG_WARNING(
        "Relay[%d] turn ON ignored, not fully initialized",
        channel.getChannelNumber());
    return;
  }

  SUPLA_LOG_INFO(
            "Relay[%d] turn ON (duration %d ms)",
            channel.getChannelNumber(),
            duration);

  applyDuration(duration, true);

  if (pin >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOnValue(), io);
  }

  channel.setRelayOvercurrentCutOff(false);
  setNewChannelValue(true);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
}

void Relay::applyDuration(int duration, bool turnOn) {
  if (isCyclicMode() && duration > 0) {
    if (turnOn) {
      storedTurnOnDurationMs = duration;
    } else {
      turnOffDurationForCycle = duration;
    }
  }
  durationMs = duration;

  if (turnOn) {
    if (minimumAllowedDurationMs > 0 && storedTurnOnDurationMs == 0) {
      storedTurnOnDurationMs = durationMs;
    }

    if (keepTurnOnDurationMs || isStaircaseFunction() || isImpulseFunction()) {
      durationMs = storedTurnOnDurationMs;
    }
  }

  if (durationMs != 0) {
    durationTimestamp = millis();
  } else {
    durationTimestamp = 0;
  }
}

void Relay::turnOff(_supla_int_t duration) {
  if (!isFullyInitialized()) {
    SUPLA_LOG_WARNING(
        "Relay[%d] turn OFF ignored, not fully initialized",
        channel.getChannelNumber());
    return;
  }

  SUPLA_LOG_INFO(
            "Relay[%d] turn OFF (duration %d ms)",
            channel.getChannelNumber(),
            duration);

  applyDuration(duration, false);

  if (pin >= 0) {
    Supla::Io::digitalWrite(channel.getChannelNumber(), pin, pinOffValue(), io);
  }

  setNewChannelValue(false);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
}

bool Relay::isOn() {
  if (pin >= 0) {
    return Supla::Io::digitalRead(channel.getChannelNumber(), pin, io) ==
           pinOnValue();
  }
  return false;
}

void Relay::toggle(_supla_int_t duration) {
  SUPLA_LOG_DEBUG(
            "Relay[%d] toggle (duration %d ms)",
            channel.getChannelNumber(),
            duration);
  if (isOn()) {
    turnOff(isCyclicMode() ? turnOffDurationForCycle : duration);
  } else {
    turnOn(isCyclicMode() ? storedTurnOnDurationMs : duration);
  }
}

void Relay::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case TURN_ON_WITHOUT_TIMER: {
      uint32_t copyDurationMs = storedTurnOnDurationMs;
      storedTurnOnDurationMs = 0;
      SUPLA_LOG_DEBUG("Relay[%d] override stored durationMs",
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
    case TOGGLE_WITH_POSTPONED_COMM: {
      postponeCommTimestamp = millis();
      [[fallthrough]];
    }
    case TOGGLE: {
      if (isRestartTimerOnToggle() &&
          (isStaircaseFunction() || isImpulseFunction())) {
        turnOn();
      } else {
        toggle();
      }
      break;
    }
  }
}

void Relay::onSaveState() {
  uint32_t durationForState = storedTurnOnDurationMs;
  uint8_t relayFlags = 0;
  if (channel.isRelayOvercurrentCutOff()) {
    relayFlags |= RELAY_FLAGS_OVERCURRENT;
  }
  if (isStaircaseFunction()) {
    relayFlags |= RELAY_FLAGS_STAIRCASE;
  } else if (isImpulseFunction()) {
    relayFlags |= RELAY_FLAGS_IMPULSE_FUNCTION;
  } else if (isCountdownTimerFunctionEnabled() && stateOnInit < 0) {
    // for other functions we store remaining countdown timer value
    durationForState = 0;
    if (durationMs && durationTimestamp != 0) {
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
  uint32_t storedDuration = 0;
  Supla::Storage::ReadState(
      reinterpret_cast<unsigned char *>(&storedDuration),
      sizeof(storedDuration));
  if (!isCyclicMode()) {
    storedTurnOnDurationMs = storedDuration;
  }
  uint8_t relayFlags = 0;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&relayFlags),
                            sizeof(relayFlags));
  if (stateOnInit < 0) {
    SUPLA_LOG_INFO(
              "Relay[%d] restored relay state: %s",
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
              "Relay[%d] restored staircase function",
              channel.getChannelNumber());
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg) {
      setAndSaveFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
    }
  } else if (relayFlags & RELAY_FLAGS_IMPULSE_FUNCTION) {
    SUPLA_LOG_INFO(
              "Relay[%d] restored impulse function",
              channel.getChannelNumber());
    // actual funciton may be different, but we only have 8 bit bitfiled to
    // keep the state and currently it doesn't matter which "impulse function"
    // is actually used, so we set it to "controlling the gate"
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg) {
      setAndSaveFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
    }
  }
  if (relayFlags & RELAY_FLAGS_OVERCURRENT) {
    channel.setRelayOvercurrentCutOff(true);
  }

  if (isStaircaseFunction() || isImpulseFunction()) {
    SUPLA_LOG_INFO(
              "Relay[%d] restored durationMs: %d",
              channel.getChannelNumber(),
              storedTurnOnDurationMs);
    if (storedTurnOnDurationMs == 0) {
      storedTurnOnDurationMs =
          (isStaircaseFunction() ? defaultStaircaseDurationMs
                                 : defaultImpulseDurationMs);
      SUPLA_LOG_WARNING(
          "Relay[%d] restored durationMs is zero, using default value %d ms",
          channel.getChannelNumber(),
          storedTurnOnDurationMs);
    }
  } else {
    // restore remaining countdown timer value
    // relay will be on/off in onInit() with configured durationMs
    if (stateOnInit < 0) {
      durationMs = storedTurnOnDurationMs;
      SUPLA_LOG_DEBUG(
          "Relay[%d] restored remaining countdown timer durationMs: %d",
          channel.getChannelNumber(),
          durationMs);
    }
    if (!isCyclicMode()) {
      storedTurnOnDurationMs = 0;
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
  (void)(keep);
  // empty method left for compatibility
  // DEPREACATED
  return *this;
}

unsigned _supla_int_t Relay::getStoredTurnOnDurationMs() {
  return storedTurnOnDurationMs;
}

void Relay::setStoredTurnOnDurationMs(uint32_t timeMs) {
  storedTurnOnDurationMs = timeMs;
}

void Relay::attach(Supla::Control::Button *button) {
  if (button == nullptr) {
    return;
  }

  SUPLA_LOG_DEBUG("Relay[%d] attaching button %d", channel.getChannelNumber(),
                  button->getButtonNumber());
  auto lastButtonListElement = buttonList;
  while (lastButtonListElement && lastButtonListElement->next) {
    lastButtonListElement = lastButtonListElement->next;
  }

  if (lastButtonListElement) {
    lastButtonListElement->next = new ButtonListElement;
    lastButtonListElement = lastButtonListElement->next;
  } else {
    lastButtonListElement = new ButtonListElement;
  }

  lastButtonListElement->button = button;

  if (buttonList == nullptr) {
    buttonList = lastButtonListElement;
  }
}


bool Relay::isStaircaseFunction(uint32_t functionToCheck) const {
  if (functionToCheck == 0) {
    functionToCheck = getChannel()->getDefaultFunction();
  }
  return functionToCheck == SUPLA_CHANNELFNC_STAIRCASETIMER;
}

bool Relay::isImpulseFunction(uint32_t functionToCheck) const {
  if (functionToCheck == 0) {
    functionToCheck = getChannel()->getDefaultFunction();
  }
  return (functionToCheck == SUPLA_CHANNELFNC_CONTROLLINGTHEGATE ||
          functionToCheck == SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK ||
          functionToCheck == SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK ||
          functionToCheck == SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR);
}

bool Relay::setAndSaveFunction(uint32_t newFunction) {
  auto previousFunction = getChannel()->getDefaultFunction();
  bool wasImpulseFunction = isImpulseFunction();
  bool wasStaircaseFunction = isStaircaseFunction();

  bool functionChanged =
      Supla::ElementWithChannelActions::setAndSaveFunction(newFunction);

  if (wasImpulseFunction != isImpulseFunction()) {
    Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
  }
  if (wasStaircaseFunction != isStaircaseFunction()) {
    Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
  }

  if (isStaircaseFunction() || isImpulseFunction()) {
    keepTurnOnDurationMs = true;
    if (initDone) {
      if (isStaircaseFunction() && !wasStaircaseFunction) {
        storedTurnOnDurationMs = defaultStaircaseDurationMs;
      }
      if (isImpulseFunction() && !wasImpulseFunction) {
        storedTurnOnDurationMs = defaultImpulseDurationMs;
      }
    }
  } else {
    keepTurnOnDurationMs = false;
    if (!isCyclicMode()) {
      storedTurnOnDurationMs = 0;
    }
  }
  if (isStaircaseFunction()) {
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_EXTENDED);
  } else {
    usedConfigTypes.clear(SUPLA_CONFIG_TYPE_EXTENDED);
  }

  if (functionChanged && previousFunction != 0) {
    turnOff();
  }

  return functionChanged;
}

void Relay::updateTimerValue() {
  uint32_t remainingTime = 0;
  uint8_t state = 0;
  int32_t senderId = 0;

  if (durationMs != 0 && durationTimestamp != 0) {
    uint32_t elapsedTimeMs = millis() - durationTimestamp;
    if (elapsedTimeMs <= durationMs) {
      remainingTime = durationMs - elapsedTimeMs;
      state = (isOn() ? 0 : 1);
    }
  }

  SUPLA_LOG_DEBUG("Relay[%d] updating timer value: remainingTime=%d state=%d",
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

void Relay::setMinimumAllowedDurationMs(uint32_t timeMs) {
  if (timeMs > UINT16_MAX) {
    timeMs = UINT16_MAX;
  }
  minimumAllowedDurationMs = timeMs;
}

void Relay::fillChannelConfig(void *channelConfig,
                              int *size,
                              uint8_t configType) {
  if (size) {
    *size = 0;
  } else {
    return;
  }

  if (channelConfig == nullptr) {
    return;
  }

  if (configType != SUPLA_CONFIG_TYPE_DEFAULT &&
      configType != SUPLA_CONFIG_TYPE_EXTENDED) {
    return;
  }

  auto func = channel.getDefaultFunction();
  if (func == SUPLA_CHANNELFNC_CONTROLLINGTHEGATE ||
      func == SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK ||
      func == SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR ||
      func == SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK) {
    SUPLA_LOG_DEBUG(
        "Relay[%d] fill channel config for impulse functions - missing "
        "implementation",
        channel.getChannelNumber());
    // TODO(klew): add
  } else if (func == SUPLA_CHANNELFNC_PUMPSWITCH ||
             func == SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH) {
      SUPLA_LOG_DEBUG(
          "Relay[%d] fill channel config for hvac related functions - missing "
          "implementation",
          channel.getChannelNumber());
      // TODO(klew): add
  } else if (func == SUPLA_CHANNELFNC_STAIRCASETIMER &&
             configType == SUPLA_CONFIG_TYPE_DEFAULT) {
    SUPLA_LOG_DEBUG("Relay[%d] fill channel config for staircase function",
                    channel.getChannelNumber());
    auto config =
        reinterpret_cast<TChannelConfig_StaircaseTimer *>(channelConfig);
    *size = sizeof(TChannelConfig_StaircaseTimer);
    config->TimeMS = storedTurnOnDurationMs;
  } else if (((func == SUPLA_CHANNELFNC_LIGHTSWITCH ||
               func == SUPLA_CHANNELFNC_POWERSWITCH) &&
              configType == SUPLA_CONFIG_TYPE_DEFAULT) ||
             (func == SUPLA_CHANNELFNC_STAIRCASETIMER &&
              configType == SUPLA_CONFIG_TYPE_EXTENDED)) {
    SUPLA_LOG_DEBUG(
        "Relay[%d] fill channel config for power switch functions (or ext for "
        "staircase)",
        channel.getChannelNumber());
    auto config = reinterpret_cast<TChannelConfig_PowerSwitch *>(channelConfig);
    *size = sizeof(TChannelConfig_PowerSwitch);
    config->OvercurrentMaxAllowed = overcurrentMaxAllowed;
    config->OvercurrentThreshold = overcurrentThreshold;
    config->DefaultRelatedMeterChannelNo = 0;
    config->DefaultRelatedMeterIsSet = 0;
    if (defaultRelatedMeterChannelNo >= 0 &&
        defaultRelatedMeterChannelNo <= 255) {
      config->DefaultRelatedMeterChannelNo = defaultRelatedMeterChannelNo;
      config->DefaultRelatedMeterIsSet = 1;
      }
  } else {
    SUPLA_LOG_WARNING("Relay[%d] fill channel config for unknown function %d",
                      channel.getChannelNumber(),
                      channel.getDefaultFunction());
    return;
  }
}

void Relay::setDefaultRelatedMeterChannelNo(int channelNo) {
  if (channelNo >= 0 && channelNo <= 255) {
    SUPLA_LOG_DEBUG("Relay[%d] DefaultRelatedMeterChannelNo set to %d",
        channel.getChannelNumber(), channelNo);
    defaultRelatedMeterChannelNo = channelNo;
  }
}

void Relay::updateRelayHvacAggregator() {
  auto channelFunction = getChannel()->getDefaultFunction();
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_PUMPSWITCH:
    case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH: {
      auto ptr = Supla::Control::RelayHvacAggregator::Add(
          getChannelNumber(), this);
      if (ptr) {
        ptr->setTurnOffWhenEmpty(turnOffWhenEmptyAggregator);
      }
      return;
    }
    default: {}
  }
  Supla::Control::RelayHvacAggregator::Remove(getChannelNumber());
}

void Relay::setTurnOffWhenEmptyAggregator(bool turnOff) {
  turnOffWhenEmptyAggregator = turnOff;
}

bool Relay::isDefaultRelatedMeterChannelSet() const {
  if (defaultRelatedMeterChannelNo >= 0 &&
      defaultRelatedMeterChannelNo <= 255) {
    auto ch = Supla::Channel::GetByChannelNumber(defaultRelatedMeterChannelNo);
    if (ch && ch->getChannelType() == SUPLA_CHANNELTYPE_ELECTRICITY_METER) {
      return true;
    }
  }
  return false;
}


uint32_t Relay::getCurrentValueFromMeter() const {
  if (isDefaultRelatedMeterChannelSet()) {
    auto el =
      Supla::Element::getElementByChannelNumber(defaultRelatedMeterChannelNo);
    if (el) {
      auto getter = EmCurrent(0);
      if (getter == nullptr) {
        return 0;
      }
      bool isValid = true;
      auto value = getter->getValue(el, &isValid);
      delete getter;
      getter = nullptr;
      if (!isValid) {
        return 0;
      }

      return value * 100;
    }
  }
  return 0;
}

void Relay::setOvercurrentMaxAllowed(uint32_t value) {
  overcurrentMaxAllowed = value;
}

void Relay::setOvercurrentThreshold(uint32_t value) {
  if (value > overcurrentMaxAllowed) {
    value = overcurrentMaxAllowed;
  }

  if (overcurrentThreshold != value) {
    overcurrentThreshold = value;
    if (isStaircaseFunction()) {
      triggerSetChannelConfig(SUPLA_CONFIG_TYPE_EXTENDED);
    } else {
      triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    }
    saveConfig();
  }
}

void Relay::saveConfig() const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RelayOvercurrentThreshold);
    if (cfg->setUInt32(key, overcurrentThreshold)) {
      SUPLA_LOG_INFO("Relay[%d]: config saved successfully",
                     getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("Relay[%d]: failed to save config", getChannelNumber());
    }

    saveConfigChangeFlag();
    cfg->saveWithDelay(5000);
  }
  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void Relay::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::RelayOvercurrentThreshold);
    cfg->eraseKey(key);
  }
}

void Relay::setRestartTimerOnToggle(bool restart) {
  restartTimerOnToggle = restart;
}

bool Relay::isRestartTimerOnToggle() const {
  return restartTimerOnToggle;
}


bool Relay::isFullyInitialized() const {
  return initDone && !skipInitialStateSetting;
}

void Relay::setNewChannelValue(bool value) {
  channel.setNewValue(value);
}

void Relay::enableCyclicMode(uint32_t turnOnTimeMs, uint32_t turnOffTimeMs) {
  SUPLA_LOG_ERROR("Relay[%d] cyclic mode enabled", channel.getChannelNumber());
  storedTurnOnDurationMs = turnOnTimeMs;
  turnOffDurationForCycle = turnOffTimeMs;
}

void Relay::disableCyclicMode() {
  storedTurnOnDurationMs = 0;
  turnOffDurationForCycle = 0;
}

bool Relay::isCyclicMode() const {
  return storedTurnOnDurationMs > 0 && turnOffDurationForCycle > 0;
}
