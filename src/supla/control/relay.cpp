// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/* Relay class
 * This class is used to control any type of relay that can be controlled
 * by setting LOW or HIGH output on selected GPIO.
 */

#include "relay.h"

#include <supla/actions.h>
#include <supla/channel_function_string.h>
#include <supla/condition.h>
#include <supla/condition_getter.h>
#include <supla/control/button.h>
#include <supla/control/relay_hvac_aggregator.h>
#include <supla/events.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/tools.h>

#include "relay_weekly_schedule.h"

using Supla::Control::Relay;

uint16_t Relay::relayStorageSaveDelay = 5000;

namespace {

Supla::Io::IoPin MakeOutputPin(Supla::Io::Base *io, int pin, bool highIsOn) {
  Supla::Io::IoPin outputPin(pin, io);
  outputPin.setActiveHigh(highIsOn);
  outputPin.setMode(OUTPUT);
  return outputPin;
}

}  // namespace

void Relay::setRelayStorageSaveDelay(uint32_t delayMs) {
  relayStorageSaveDelay = delayMs > UINT16_MAX
                              ? UINT16_MAX
                              : static_cast<uint16_t>(delayMs);
}

void Relay::fillDefaultWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule) {
  // The native Relay default is a valid no-op schedule. Derived relay classes
  // may fill programs and quarters for a device-specific default.
  (void)(schedule);
}

Relay::Relay(Supla::Io::IoPin outputPin, _supla_int_t functions)
    : outputPin(outputPin) {
  this->outputPin.setMode(OUTPUT);
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFuncList(functions);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

Relay::Relay(Supla::Io::IoPin outputPin,
             _supla_int_t functions,
             Supla::Channel &externalChannel,
             ElementMode mode)
    : ChannelElement(externalChannel, mode), outputPin(outputPin) {
  this->outputPin.setMode(OUTPUT);
  channel.setType(SUPLA_CHANNELTYPE_RELAY);
  channel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFuncList(functions);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

Relay::Relay(Supla::Io::Base *io,
             int pin,
             bool highIsOn,
             _supla_int_t functions)
    : Relay(MakeOutputPin(io, pin, highIsOn), functions) {
}

Relay::Relay(int pin, bool highIsOn, _supla_int_t functions)
    : Relay(MakeOutputPin(nullptr, pin, highIsOn), functions) {
}

Relay::~Relay() {
  ButtonListElement *currentElement = buttonList;
  while (currentElement) {
    ButtonListElement *nextElement = currentElement->next;
    delete currentElement;
    currentElement = nextElement;
  }
  Supla::Control::RelayHvacAggregator::Remove(getChannelNumber());
}

bool Relay::setWeeklyScheduleController(
    WeeklyScheduleController *controller,
    WeeklyScheduleConfigHandler *configHandler,
    WeeklyScheduleProgramSource *programSource) {
  if (!weeklyScheduleComponents.set(
          controller, configHandler, programSource)) {
    return false;
  }
  usedConfigTypes.clear(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  usedConfigTypes.clear(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE);
  if (configHandler) {
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE,
                        configHandler->supportsConfigType(
                            SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE));
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE,
                        configHandler->supportsConfigType(
                            SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE));
  }
  return true;
}

void Relay::onLoadConfig(SuplaDeviceClass *) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();
  }
  loadRelayConfigOnly();
  if (cfg) {
    // Load pending config types after WEEKLY/EXTENDED support is known.
    loadConfigChangeFlag();
  }
}

void Relay::loadRelayConfigOnly() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
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
  updateWeeklyScheduleCapabilities();
  if (weeklyScheduleComponents.isAssigned()) {
    weeklyScheduleComponents.loadConfig();
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

bool Relay::ensureNativeWeeklyScheduleController() {
  if (!isWeeklyScheduleSupported() ||
      weeklyScheduleComponents.isAssigned()) {
    return false;
  }

  auto *controller = new RelayWeeklySchedule(this);
  if (!setWeeklyScheduleController(controller, controller, controller)) {
    delete controller;
    return false;
  }
  return true;
}

void Relay::updateWeeklyScheduleCapabilities() {
  ensureNativeWeeklyScheduleController();
  auto *controller = weeklyScheduleComponents.getController();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  const bool configurableWeeklySchedule =
      configHandler && configHandler->supportsConfigType(
                           SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  const bool externalWeeklySchedule =
      controller && controller->isExternallyManaged();
  if (isWeeklyScheduleSupported() && configurableWeeklySchedule) {
    channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    if (isWeeklyScheduleProgramModeSupported(SUPLA_RELAY_MODE_ON_ONCE) &&
        isWeeklyScheduleProgramModeSupported(SUPLA_RELAY_MODE_OFF_ONCE)) {
      channel.setFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_ONCE_SUPPORTED);
    } else {
      channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_ONCE_SUPPORTED);
    }
    if (isWeeklyScheduleProgramModeSupported(SUPLA_RELAY_MODE_FORCED_ON) &&
        isWeeklyScheduleProgramModeSupported(SUPLA_RELAY_MODE_FORCED_OFF)) {
      channel.setFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED);
    } else {
      channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED);
    }
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  } else if (isWeeklyScheduleSupported() && externalWeeklySchedule) {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_ONCE_SUPPORTED);
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED);
    usedConfigTypes.clear(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  } else {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_ONCE_SUPPORTED);
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_FORCED_SUPPORTED);
    usedConfigTypes.clear(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
    if (controller != nullptr) {
      controller->switchToManualMode();
    } else {
      channel.setRelayWeeklyScheduleEnabled(false);
    }
  }
  updateAutomaticModeCapability();
}

void Relay::updateAutomaticModeCapability() {
  auto *controller = weeklyScheduleComponents.getController();
  if (isAutomaticModeSupported() ||
      (isWeeklyScheduleSupported() && controller != nullptr &&
       controller->isExternallyManaged())) {
    channel.setFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED);
  } else {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_RELAY_MODE_AUTOMATIC_SUPPORTED);
  }
}

void Relay::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::ElementWithChannelActions::onRegistered(suplaSrpc);

  timerUpdateTimestamp = 1;
}

Supla::ApplyConfigResult Relay::applyChannelConfig(TSD_ChannelConfig *result,
                                                   bool local) {
  SUPLA_LOG_DEBUG(
      "Relay[%d] applyChannelConfig, func %s (%d), configtype %d, configsize "
      "%d",
      getChannelNumber(),
      Supla::channelFunctionToString(result->Func),
      result->Func,
      result->ConfigType,
      result->ConfigSize);

  updateRelayHvacAggregator();

  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  if (configHandler != nullptr &&
      result->ConfigType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE &&
      isWeeklyScheduleSupported()) {
    return configHandler->applyChannelConfig(result, local);
  }

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
        setOvercurrentThreshold(config->OvercurrentThreshold, local);
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
        SUPLA_LOG_DEBUG(
            "Relay[%d] missing StaircaseTimer duration, setting to "
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
  return outputPin.isActiveHigh() ? HIGH : LOW;
}

uint8_t Relay::pinOffValue() {
  return outputPin.isActiveHigh() ? LOW : HIGH;
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
        if (attachedButton->isMotionSensor() || attachedButton->isCentral()) {
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
    if (!isLastResetSoft() || preloadStateOnSoftReset) {
      if (stateOn) {
        turnOn(duration);
      } else {
        turnOff(duration);
      }
    }

    // pin mode is set after setting pin value in order to
    // avoid problems with LOW trigger relays
    outputPin.pinMode(channel.getChannelNumber());

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

  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr && isWeeklyScheduleSupported()) {
    weeklySchedule->processWeeklySchedule();
  }
  if (isAutomaticModeSupported() && isAutomaticMode()) {
    iterateAutomaticMode();
  }

  if (durationMs && millis() - durationTimestamp > durationMs) {
    toggle();
  }
  emitCountdownTimerActionIfNeeded();

  if (overcurrentThreshold > 0 && isOn()) {
    if (millis() - overcurrentCheckTimestamp > 500) {
      overcurrentCheckTimestamp = millis();
      auto current = getCurrentValueFromMeter();
      if (current > overcurrentThreshold * 1.2) {
        SUPLA_LOG_WARNING(
            "Relay[%d] Overcurrent detected (%d) - instant turn off",
            getChannelNumber(),
            current);
        channel.setRelayOvercurrentCutOff(true);
        turnOff();
        return;
      }
      if (current >= overcurrentThreshold) {
        if (overcurrentActiveTimestamp != 0 &&
            millis() - overcurrentActiveTimestamp > 30 * 1000) {  // 30 s
          SUPLA_LOG_WARNING("Relay[%d] Overcurrent detected (%d) - turn off",
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
          SUPLA_LOG_DEBUG("Relay[%d] Overcurrent filtering cancelled (%d)",
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

bool Relay::getRemainingCountdownTimerSec(uint32_t *remainingSec) const {
  if (remainingSec) {
    *remainingSec = 0;
  }
  if (!isCountdownTimerFunctionEnabled() || durationMs == 0 ||
      durationTimestamp == 0) {
    return false;
  }

  uint32_t elapsedMs = millis() - durationTimestamp;
  if (elapsedMs >= durationMs) {
    return false;
  }

  uint32_t remainingMs = durationMs - elapsedMs;
  if (remainingSec) {
    *remainingSec = (remainingMs + 999) / 1000;
  }
  return true;
}

void Relay::emitCountdownTimerActionIfNeeded() {
  uint32_t remainingSec = UINT32_MAX;
  uint32_t currentRemainingSec = 0;
  if (getRemainingCountdownTimerSec(&currentRemainingSec)) {
    remainingSec = currentRemainingSec;
  }
  if (remainingSec != lastCountdownTimerRemainingSec) {
    lastCountdownTimerRemainingSec = remainingSec;
    runAction(Supla::ON_COUNTDOWN_TIMER);
  }
}

bool Relay::iterateConnected() {
  if (postponeCommTimestamp != 0 && millis() - postponeCommTimestamp < 500) {
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
  auto *relayValue = reinterpret_cast<TRelayChannel_Value *>(newValue->value);
  auto *weeklySchedule = weeklyScheduleComponents.getController();
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
    default: {
    }
  }

  if (relayValue->RelayMode == SUPLA_RELAY_MODE_AUTOMATIC) {
    if (!setAutomaticMode(true)) {
      return 0;
    }
    Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
    return 1;
  }

  if (relayValue->RelayMode == SUPLA_RELAY_MODE_CMD_SWITCH_TO_MANUAL &&
      (isAutomaticMode() ||
       (weeklySchedule != nullptr && isWeeklyScheduleSupported()))) {
    setAutomaticMode(false);
    Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
    return 1;
  }

  if (weeklySchedule != nullptr && isWeeklyScheduleSupported()) {
    switch (relayValue->RelayMode) {
      case SUPLA_RELAY_MODE_CMD_WEEKLY_SCHEDULE: {
        if (weeklySchedule->switchToWeeklySchedule()) {
          if (weeklySchedule->isExternallyManaged()) {
            channel.setRelayWeeklyScheduleEnabled(
                weeklySchedule->isActive());
            channel.setRelayMode(weeklySchedule->isActive()
                                     ? SUPLA_RELAY_MODE_AUTOMATIC
                                     : SUPLA_RELAY_MODE_NOT_SET);
          }
          Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
          return 1;
        }
        return 0;
      }
      default: {
      }
    }
  }

  int result = -1;
  if (newValue->value[0] == 1) {
    if (!isManualActionAllowedByWeeklySchedule(true)) {
      SUPLA_LOG_DEBUG(
          "Relay[%d] ignoring server turn ON due to weekly schedule",
          channel.getChannelNumber());
      return 0;
    }
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
    if (!isManualActionAllowedByWeeklySchedule(false)) {
      SUPLA_LOG_DEBUG(
          "Relay[%d] ignoring server turn OFF due to weekly schedule",
          channel.getChannelNumber());
      return 0;
    }
    if (keepTurnOnDurationMs || isStaircaseFunction() || isImpulseFunction()) {
      turnOff(0);  // newValue->DurationMS may contain "turn on duration" which
                   // result in unexpected "turn on after duration ms received
                   // in turnOff message"
    } else {
      // Cyclic-mode semantics are intentional here:
      //
      // OFF with DurationMS > 0 configures/starts the OFF phase of the cycle.
      // After DurationMS expires, the relay is expected to turn ON again.
      //
      // OFF with DurationMS == 0 is the explicit "stop cycle" command and
      // leaves the relay OFF because no timer is armed.
      //
      // Do not replace this with turnOff(0) for all cyclic-mode OFF commands.
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
    SUPLA_LOG_WARNING("Relay[%d] turn ON ignored, not fully initialized",
                      channel.getChannelNumber());
    return;
  }

  SUPLA_LOG_INFO("Relay[%d] turn ON (duration %d ms)",
                 channel.getChannelNumber(),
                 duration);

  applyDuration(duration, true);

  outputPin.writeActive(channel.getChannelNumber());

  channel.setRelayOvercurrentCutOff(false);
  setNewChannelValue(true);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
}

void Relay::applyDuration(int32_t duration, bool turnOn) {
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
    if (durationTimestamp == 0) {
      durationTimestamp = UINT32_MAX;
    }
  } else {
    durationTimestamp = 0;
  }
}

void Relay::turnOff(_supla_int_t duration) {
  if (!isFullyInitialized()) {
    SUPLA_LOG_WARNING("Relay[%d] turn OFF ignored, not fully initialized",
                      channel.getChannelNumber());
    return;
  }

  SUPLA_LOG_INFO("Relay[%d] turn OFF (duration %d ms)",
                 channel.getChannelNumber(),
                 duration);

  applyDuration(duration, false);

  outputPin.writeInactive(channel.getChannelNumber());

  setNewChannelValue(false);

  // Schedule save in 5 s after state change
  Supla::Storage::ScheduleSave(relayStorageSaveDelay, 2000);
}

bool Relay::isOn() {
  return outputPin.readActive(channel.getChannelNumber());
}

void Relay::toggle(_supla_int_t duration) {
  SUPLA_LOG_DEBUG("Relay[%d] toggle (duration %d ms)",
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
      if (!isManualActionAllowedByWeeklySchedule(true)) {
        SUPLA_LOG_DEBUG("Relay[%d] ignoring TURN_ON due to weekly schedule",
                        channel.getChannelNumber());
        return;
      }
      uint32_t copyDurationMs = storedTurnOnDurationMs;
      storedTurnOnDurationMs = 0;
      SUPLA_LOG_DEBUG("Relay[%d] override stored durationMs",
                      channel.getChannelNumber());
      turnOn();
      storedTurnOnDurationMs = copyDurationMs;
      break;
    }
    case TURN_ON: {
      if (!isManualActionAllowedByWeeklySchedule(true)) {
        SUPLA_LOG_DEBUG("Relay[%d] ignoring TURN_ON due to weekly schedule",
                        channel.getChannelNumber());
        return;
      }
      turnOn();
      break;
    }
    case TURN_OFF: {
      if (!isManualActionAllowedByWeeklySchedule(false)) {
        SUPLA_LOG_DEBUG("Relay[%d] ignoring TURN_OFF due to weekly schedule",
                        channel.getChannelNumber());
        return;
      }
      turnOff();
      break;
    }
    case TOGGLE_WITH_POSTPONED_COMM: {
      auto *weeklySchedule = weeklyScheduleComponents.getController();
      if (weeklySchedule && isWeeklyScheduleSupported()) {
        if (isOn() &&
            !weeklySchedule->isManualActionAllowed(false)) {
          SUPLA_LOG_DEBUG(
              "Relay[%d] ignoring TOGGLE-off due to weekly schedule",
              channel.getChannelNumber());
          return;
        }
        if (!isOn() &&
            !weeklySchedule->isManualActionAllowed(true)) {
          SUPLA_LOG_DEBUG("Relay[%d] ignoring TOGGLE-on due to weekly schedule",
                          channel.getChannelNumber());
          return;
        }
      }
      postponeCommTimestamp = millis();
      [[fallthrough]];
    }
    case TOGGLE: {
      auto *weeklySchedule = weeklyScheduleComponents.getController();
      if (weeklySchedule && isWeeklyScheduleSupported()) {
        if (isOn() &&
            !weeklySchedule->isManualActionAllowed(false)) {
          SUPLA_LOG_DEBUG(
              "Relay[%d] ignoring TOGGLE-off due to weekly schedule",
              channel.getChannelNumber());
          return;
        }
        if (!isOn() &&
            !weeklySchedule->isManualActionAllowed(true)) {
          SUPLA_LOG_DEBUG("Relay[%d] ignoring TOGGLE-on due to weekly schedule",
                          channel.getChannelNumber());
          return;
        }
      }
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
  RelayFlags relayFlags;
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  relayFlags.flags.overcurrent = channel.isRelayOvercurrentCutOff();
  relayFlags.flags.weeklySchedule =
      weeklySchedule != nullptr && isWeeklyScheduleSupported() &&
      weeklySchedule->isActive();
  relayFlags.flags.automaticMode =
      isAutomaticMode() && !relayFlags.flags.weeklySchedule;
  if (isStaircaseFunction()) {
    relayFlags.flags.staircaseFunction = 1;
  } else if (isImpulseFunction()) {
    relayFlags.flags.impulseFunction = 1;
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
    relayFlags.flags.relayOn = isOn() ? 1 : 0;
  }

  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&relayFlags),
                             sizeof(relayFlags));
}

void Relay::onLoadState() {
  uint32_t storedDuration = 0;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&storedDuration),
                            sizeof(storedDuration));
  if (!isCyclicMode()) {
    storedTurnOnDurationMs = storedDuration;
  }
  RelayFlags relayFlags;
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&relayFlags),
                            sizeof(relayFlags));
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  bool restoreOn = relayFlags.flags.relayOn;
  if (restoreOn &&
      (relayFlags.flags.impulseFunction || isImpulseFunction())) {
    SUPLA_LOG_INFO(
        "Relay[%d] ignoring restored ON state for impulse function",
        channel.getChannelNumber());
    restoreOn = false;
  }
  if (stateOnInit < 0) {
    SUPLA_LOG_INFO("Relay[%d] restored relay state: %s",
                   channel.getChannelNumber(),
                   restoreOn ? "ON" : "OFF");
    if (restoreOn) {
      stateOnInit = STATE_ON_INIT_RESTORED_ON;
    } else {
      stateOnInit = STATE_ON_INIT_RESTORED_OFF;
    }
  }
  if (relayFlags.flags.staircaseFunction) {
    SUPLA_LOG_INFO("Relay[%d] restored staircase function",
                   channel.getChannelNumber());
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg) {
      setAndSaveFunction(SUPLA_CHANNELFNC_STAIRCASETIMER);
    }
  } else if (relayFlags.flags.impulseFunction) {
    SUPLA_LOG_INFO("Relay[%d] restored impulse function",
                   channel.getChannelNumber());
    // actual funciton may be different, but we only have 8 bit bitfiled to
    // keep the state and currently it doesn't matter which "impulse function"
    // is actually used, so we set it to "controlling the gate"
    auto cfg = Supla::Storage::ConfigInstance();
    if (!cfg) {
      setAndSaveFunction(SUPLA_CHANNELFNC_CONTROLLINGTHEGATE);
    }
  }
  if (relayFlags.flags.overcurrent) {
    channel.setRelayOvercurrentCutOff(true);
  }
  if (weeklySchedule != nullptr) {
    weeklySchedule->restoreWeeklyScheduleMode(
        relayFlags.flags.weeklySchedule && isWeeklyScheduleSupported());
    if (weeklySchedule->isExternallyManaged()) {
      channel.setRelayWeeklyScheduleEnabled(
          weeklySchedule->isActive());
      channel.setRelayMode(weeklySchedule->isActive()
                               ? SUPLA_RELAY_MODE_AUTOMATIC
                               : SUPLA_RELAY_MODE_NOT_SET);
    }
  }
  if (!relayFlags.flags.weeklySchedule && relayFlags.flags.automaticMode &&
      isAutomaticModeSupported()) {
    setAutomaticMode(true);
  }

  if (isStaircaseFunction() || isImpulseFunction()) {
    SUPLA_LOG_INFO("Relay[%d] restored durationMs: %d",
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

Relay &Relay::setPreloadStateOnSoftReset(bool enabled) {
  preloadStateOnSoftReset = enabled;
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

  SUPLA_LOG_DEBUG("Relay[%d] attaching button %d",
                  channel.getChannelNumber(),
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

bool Relay::setRuntimeFunction(uint32_t newFunction) {
  auto previousFunction = getChannel()->getDefaultFunction();
  bool wasImpulseFunction = isImpulseFunction();
  bool wasStaircaseFunction = isStaircaseFunction();

  bool functionChanged = Supla::Element::setRuntimeFunction(newFunction);

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

  bool weeklyScheduleControllerCreated =
      ensureNativeWeeklyScheduleController();
  updateWeeklyScheduleCapabilities();
  if (weeklyScheduleControllerCreated && initDone &&
      !weeklyScheduleComponents.isStarted()) {
    weeklyScheduleComponents.loadConfig();
  }

  if (functionChanged && previousFunction != 0) {
    turnOff();
  }

  return functionChanged;
}

bool Relay::setAndSaveFunction(uint32_t newFunction) {
  return Supla::ElementWithChannelActions::setAndSaveFunction(newFunction);
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

  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
       proto = proto->next()) {
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

  if (configType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    ensureNativeWeeklyScheduleController();
    updateWeeklyScheduleCapabilities();
    auto *configHandler = weeklyScheduleComponents.getConfigHandler();
    if (configHandler != nullptr && isWeeklyScheduleSupported()) {
      configHandler->fillChannelConfig(channelConfig, size, configType);
    }
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
    SUPLA_LOG_WARNING(
        "Relay[%d] fill channel config for unknown function %s (%d)",
        channel.getChannelNumber(),
        Supla::channelFunctionToString(channel.getDefaultFunction()),
        channel.getDefaultFunction());
    return;
  }
}

void Relay::setDefaultRelatedMeterChannelNo(int channelNo) {
  if (channelNo >= 0 && channelNo <= 255) {
    SUPLA_LOG_DEBUG("Relay[%d] DefaultRelatedMeterChannelNo set to %d",
                    channel.getChannelNumber(),
                    channelNo);
    defaultRelatedMeterChannelNo = channelNo;
  }
}

void Relay::updateRelayHvacAggregator() {
  auto channelFunction = getChannel()->getDefaultFunction();
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_PUMPSWITCH:
    case SUPLA_CHANNELFNC_HEATORCOLDSOURCESWITCH: {
      auto ptr =
          Supla::Control::RelayHvacAggregator::Add(getChannelNumber(), this);
      if (ptr) {
        ptr->setTurnOffWhenEmpty(turnOffWhenEmptyAggregator);
      }
      return;
    }
    default: {
    }
  }
  Supla::Control::RelayHvacAggregator::Remove(getChannelNumber());
}

void Relay::setTurnOffWhenEmptyAggregator(bool turnOff) {
  turnOffWhenEmptyAggregator = turnOff;
}

bool Relay::isWeeklyScheduleSupported() const {
  auto func = channel.getDefaultFunction();
  return func == SUPLA_CHANNELFNC_LIGHTSWITCH ||
         func == SUPLA_CHANNELFNC_POWERSWITCH ||
         isStaircaseFunction(func) || isImpulseFunction(func);
}

Relay &Relay::setAutomaticModeSupported(bool supported) {
  automaticModeSupported = supported;
  updateAutomaticModeCapability();
  auto *controller = weeklyScheduleComponents.getController();
  const bool activeExternalWeeklySchedule =
      controller != nullptr && controller->isExternallyManaged() &&
      controller->isActive();
  if (!supported && isAutomaticMode() && !activeExternalWeeklySchedule) {
    setAutomaticMode(false);
  }
  return *this;
}

bool Relay::isAutomaticModeSupported() const {
  return automaticModeSupported;
}

bool Relay::isAutomaticMode() const {
  return channel.getRelayMode() == SUPLA_RELAY_MODE_AUTOMATIC;
}

bool Relay::setAutomaticMode(bool enabled) {
  if (enabled && !isAutomaticModeSupported()) {
    return false;
  }
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr && weeklySchedule->isActive()) {
    weeklySchedule->switchToManualMode();
  }
  channel.setRelayWeeklyScheduleEnabled(false);
  channel.setRelayMode(enabled ? SUPLA_RELAY_MODE_AUTOMATIC
                               : SUPLA_RELAY_MODE_NOT_SET);
  return true;
}

void Relay::iterateAutomaticMode() {
}

bool Relay::isManualActionAllowedByWeeklySchedule(bool turnOn) const {
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  return !isWeeklyScheduleSupported() || weeklySchedule == nullptr ||
         weeklySchedule->isManualActionAllowed(turnOn);
}

bool Relay::isWeeklyScheduleProgramModeSupported(uint8_t mode) const {
  if (!isWeeklyScheduleSupported()) {
    return false;
  }

  switch (mode) {
    case SUPLA_RELAY_MODE_NOT_SET:
    case SUPLA_RELAY_MODE_OFF_ONCE:
    case SUPLA_RELAY_MODE_ON_ONCE: {
      return true;
    }
    case SUPLA_RELAY_MODE_FORCED_ON:
    case SUPLA_RELAY_MODE_FORCED_OFF: {
      // Forced modes are advertised as a pair. FORCED_ON would repeatedly
      // retrigger an impulse function after its duration expires, so impulse
      // functions support neither forced mode.
      return !isImpulseFunction();
    }
    case SUPLA_RELAY_MODE_AUTOMATIC: {
      return isAutomaticModeSupported();
    }
    default: {
      return false;
    }
  }
}

void Relay::applyWeeklyScheduleProgram(uint8_t programMode,
                                       bool programChanged) {
  switch (programMode) {
    case SUPLA_RELAY_MODE_ON_ONCE: {
      if (programChanged && !isOn() &&
          !getChannel()->isRelayOvercurrentCutOff()) {
        turnOn();
      }
      return;
    }
    case SUPLA_RELAY_MODE_OFF_ONCE: {
      if (programChanged && isOn()) {
        turnOff();
      }
      return;
    }
    case SUPLA_RELAY_MODE_FORCED_ON: {
      if (!getChannel()->isRelayOvercurrentCutOff() && !isOn()) {
        if (isStaircaseFunction()) {
          // FORCED_ON keeps a staircase relay continuously active. Preserve
          // its configured duration for manual operation after forced mode.
          const auto storedDuration = storedTurnOnDurationMs;
          storedTurnOnDurationMs = 0;
          turnOn();
          storedTurnOnDurationMs = storedDuration;
        } else {
          turnOn();
        }
      }
      return;
    }
    case SUPLA_RELAY_MODE_FORCED_OFF: {
      if (isOn()) {
        turnOff();
      }
      return;
    }
    case SUPLA_RELAY_MODE_AUTOMATIC:
    case SUPLA_RELAY_MODE_NOT_SET: {
      // The weekly schedule does not force the relay state. Target-specific
      // automatic logic runs from iterateAutomaticMode().
      return;
    }
    default: {
      return;
    }
  }
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
  setOvercurrentThreshold(value, true);
}

void Relay::setOvercurrentThreshold(uint32_t value, bool local) {
  if (value > overcurrentMaxAllowed) {
    value = overcurrentMaxAllowed;
  }

  if (overcurrentThreshold != value) {
    overcurrentThreshold = value;
    if (isStaircaseFunction()) {
      triggerSetChannelConfig(SUPLA_CONFIG_TYPE_EXTENDED, local);
    } else {
      triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, local);
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
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
       proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void Relay::purgeConfig() {
  Supla::ChannelElement::purgeConfig();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  if (configHandler) {
    configHandler->purgeConfig();
  }
  purgeRelayConfigOnly();
}

void Relay::purgeRelayConfigOnly() {
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
