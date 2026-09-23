// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_trigger.h"

#include <SuplaDevice.h>
#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla/local_action.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/control/button.h>
#include <supla/events.h>
#include <supla/storage/config_tags.h>

namespace {
// Recognize an actual directional pair, not arbitrary conditional handlers.
bool isDirectionalHandler(const Supla::ActionHandlerClient *handler) {
  int press = -1;
  int release = -1;
  switch (handler->action) {
    case Supla::MOVE_UP:
    case Supla::UP_STOP:
      press = Supla::MOVE_UP;
      release = Supla::UP_STOP;
      break;
    case Supla::MOVE_DOWN:
    case Supla::DOWN_STOP:
      press = Supla::MOVE_DOWN;
      release = Supla::DOWN_STOP;
      break;
    case Supla::INTERNAL_BUTTON_MOVE_UP:
    case Supla::INTERNAL_BUTTON_UP_STOP:
      press = Supla::INTERNAL_BUTTON_MOVE_UP;
      release = Supla::INTERNAL_BUTTON_UP_STOP;
      break;
    case Supla::INTERNAL_BUTTON_MOVE_DOWN:
    case Supla::INTERNAL_BUTTON_DOWN_STOP:
      press = Supla::INTERNAL_BUTTON_MOVE_DOWN;
      release = Supla::INTERNAL_BUTTON_DOWN_STOP;
      break;
    default:
      return false;
  }
  const bool isPress = handler->action == press &&
      handler->onEvent == Supla::CONDITIONAL_ON_PRESS;
  const bool isRelease = handler->action == release &&
      handler->onEvent == Supla::CONDITIONAL_ON_RELEASE;
  if (!handler->client || (!isPress && !isRelease)) {
    return false;
  }
  for (auto other = Supla::ActionHandlerClient::begin; other;
       other = other->next) {
    if (other->trigger == handler->trigger &&
        other->client == handler->client &&
        other->action == (isPress ? release : press) &&
        other->onEvent == (isPress ? Supla::CONDITIONAL_ON_RELEASE
                                  : Supla::CONDITIONAL_ON_PRESS)) {
      return true;
    }
  }
  return false;
}
}  // namespace

Supla::Control::ActionTrigger::ActionTrigger() {
  channel.setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_ACTIONTRIGGER);
}

Supla::Control::ActionTrigger::~ActionTrigger() {
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button &button) {
  attach(&button);
}

void Supla::Control::ActionTrigger::handleAction(int, int action) {
  if (!enabled) {
    return;
  }
  uint32_t actionCap = getActionTriggerCap(action);

  if (actionCap & activeActionsFromServer ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer) {
    channel.pushAction(actionCap);
  }
}

Supla::Channel *Supla::Control::ActionTrigger::getChannel() {
  return &channel;
}

const Supla::Channel *Supla::Control::ActionTrigger::getChannel() const {
  return &channel;
}

void Supla::Control::ActionTrigger::activateAction(int action) {
  channel.activateAction(getActionTriggerCap(action));
}

int Supla::Control::ActionTrigger::getActionTriggerCap(int action) {
  switch (action) {
    case SEND_AT_TURN_ON: {
      return SUPLA_ACTION_CAP_TURN_ON;
    }
    case SEND_AT_TURN_OFF: {
      return SUPLA_ACTION_CAP_TURN_OFF;
    }
    case SEND_AT_TOGGLE_x1: {
      return SUPLA_ACTION_CAP_TOGGLE_x1;
    }
    case SEND_AT_TOGGLE_x2: {
      return SUPLA_ACTION_CAP_TOGGLE_x2;
    }
    case SEND_AT_TOGGLE_x3: {
      return SUPLA_ACTION_CAP_TOGGLE_x3;
    }
    case SEND_AT_TOGGLE_x4: {
      return SUPLA_ACTION_CAP_TOGGLE_x4;
    }
    case SEND_AT_TOGGLE_x5: {
      return SUPLA_ACTION_CAP_TOGGLE_x5;
    }
    case SEND_AT_HOLD: {
      return SUPLA_ACTION_CAP_HOLD;
    }
    case SEND_AT_SHORT_PRESS_x1: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x1;
    }
    case SEND_AT_SHORT_PRESS_x2: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x2;
    }
    case SEND_AT_SHORT_PRESS_x3: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x3;
    }
    case SEND_AT_SHORT_PRESS_x4: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x4;
    }
    case SEND_AT_SHORT_PRESS_x5: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x5;
    }
  }
  return 0;
}

int Supla::Control::ActionTrigger::actionTriggerCapToActionId(
    uint32_t actionCap) {
  switch (actionCap) {
    case SUPLA_ACTION_CAP_TURN_ON: {
      return SEND_AT_TURN_ON;
    }
    case SUPLA_ACTION_CAP_TURN_OFF: {
      return SEND_AT_TURN_OFF;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x1: {
      return SEND_AT_TOGGLE_x1;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x2: {
      return SEND_AT_TOGGLE_x2;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x3: {
      return SEND_AT_TOGGLE_x3;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x4: {
      return SEND_AT_TOGGLE_x4;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x5: {
      return SEND_AT_TOGGLE_x5;
    }
    case SUPLA_ACTION_CAP_HOLD: {
      return SEND_AT_HOLD;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x1: {
      return SEND_AT_SHORT_PRESS_x1;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x2: {
      return SEND_AT_SHORT_PRESS_x2;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x3: {
      return SEND_AT_SHORT_PRESS_x3;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x4: {
      return SEND_AT_SHORT_PRESS_x4;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x5: {
      return SEND_AT_SHORT_PRESS_x5;
    }
  }
  return -1;
}

int Supla::Control::ActionTrigger::actionTriggerCapToButtonEvent(
    uint32_t actionCap) {
  switch (actionCap) {
    case SUPLA_ACTION_CAP_TURN_ON: {
      return Supla::ON_PRESS;
    }
    case SUPLA_ACTION_CAP_TURN_OFF: {
      return Supla::ON_RELEASE;
    }
    case SUPLA_ACTION_CAP_HOLD: {
      return Supla::ON_HOLD;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x1:
    case SUPLA_ACTION_CAP_TOGGLE_x1: {
      return Supla::ON_CLICK_1;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x2:
    case SUPLA_ACTION_CAP_TOGGLE_x2: {
      return Supla::ON_CLICK_2;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x3:
    case SUPLA_ACTION_CAP_TOGGLE_x3: {
      return Supla::ON_CLICK_3;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x4:
    case SUPLA_ACTION_CAP_TOGGLE_x4: {
      return Supla::ON_CLICK_4;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x5:
    case SUPLA_ACTION_CAP_TOGGLE_x5: {
      return Supla::ON_CLICK_5;
    }
  }
  return -1;
}

void Supla::Control::ActionTrigger::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::Element::onRegistered(suplaSrpc);
  // cleanup actions to be send
  while (channel.popAction()) {
  }

  channel.enableValueUpdates();
  channel.setSendGetConfig();
}

void Supla::Control::ActionTrigger::parseActiveActionsFromServer() {
  if (actionHandlingType == ActionHandlingType_PublishAllDisableAll) {
    activeActionsFromServer = 0xFFFFFFFF;
  }

  uint32_t actionsToDisable =
    activeActionsFromServer & disablesLocalOperation;
  if (attachedButton) {
    bool makeSureThatOnClick1IsDisabled = false;
    bool makeSureThatOnChangePressReleaseIsDisabled = false;

    if (activeActionsFromServer ||
        actionHandlingType == ActionHandlingType_PublishAllDisableNone ||
        alwaysUseOnClick1) {
      // disable on_press, on_release, on_change local actions and enable
      // on_click_1
      if (localHandlerForDisabledAt && localHandlerForEnabledAt) {
        localHandlerForDisabledAt->disable();
        localHandlerForEnabledAt->enable();
        makeSureThatOnChangePressReleaseIsDisabled = true;
      }
    } else {
      // enable on_press, on_release, on_change local actions and
      // disable on_click_1
      if (localHandlerForDisabledAt && localHandlerForEnabledAt) {
        localHandlerForDisabledAt->enable();
        localHandlerForEnabledAt->disable();
        makeSureThatOnClick1IsDisabled = true;
      }
    }

    if (activeActionsFromServer & SUPLA_ACTION_CAP_HOLD) {
      attachedButton->disableRepeatOnHold(1000);
    } else {
      attachedButton->enableRepeatOnHold();
    }

    for (int i = 0; i < 32; i++) {
      uint32_t actionCap = (1ULL << i);
      int eventId = actionTriggerCapToButtonEvent(actionCap);
      int actionId = actionTriggerCapToActionId(actionCap);
      if (eventId == -1 || actionId == -1) {
        continue;
      }
      if (actionCap & activeActionsFromServer ||
          actionHandlingType == ActionHandlingType_PublishAllDisableNone) {
        attachedButton->enableAction(actionId, this, eventId);
      } else {
        if (eventId != Supla::ON_CLICK_1 || !alwaysUseOnClick1) {
          attachedButton->disableAction(actionId, this, eventId);
        }
      }

      // enable/disable other actions when AT from server is disabled/enabled
      if (actionsToDisable & actionCap) {
        attachedButton->disableOtherClients(this, eventId);
        if (eventId == Supla::ON_PRESS) {
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_PRESS);
        } else if (eventId == Supla::ON_RELEASE) {
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_RELEASE);
        } else if (eventId == Supla::ON_CLICK_1) {
          attachedButton->disableOtherClients(this,
                                              Supla::ON_CHANGE);
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_CHANGE);
          if (attachedButton->isBistable()) {
            attachedButton->disableOtherClients(
                this, Supla::CONDITIONAL_ON_PRESS);
            attachedButton->disableOtherClients(
                this, Supla::CONDITIONAL_ON_RELEASE);
          }
        } else if (eventId == Supla::ON_HOLD) {
          attachedButton->disableOtherClients(this,
                                              Supla::ON_HOLD_RELEASE);
        }
      } else if (disablesLocalOperation & actionCap) {
        attachedButton->enableOtherClients(this, eventId);
        if (eventId == Supla::ON_PRESS) {
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_PRESS);
        } else if (eventId == Supla::ON_RELEASE) {
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_RELEASE);
        } else if (eventId == Supla::ON_CLICK_1) {
          attachedButton->enableOtherClients(this,
                                             Supla::ON_CHANGE);
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_CHANGE);
          if (attachedButton->isBistable() &&
              !(activeActionsFromServer &
                (SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF))) {
            attachedButton->enableOtherClients(
                this, Supla::CONDITIONAL_ON_PRESS);
            attachedButton->enableOtherClients(
                this, Supla::CONDITIONAL_ON_RELEASE);
          }
        } else if (eventId == Supla::ON_HOLD) {
          attachedButton->enableOtherClients(this,
                                             Supla::ON_HOLD_RELEASE);
        }
        if (makeSureThatOnClick1IsDisabled && eventId == Supla::ON_CLICK_1) {
          makeSureThatOnClick1IsDisabled = false;
          if (localHandlerForEnabledAt) {
            localHandlerForEnabledAt->disable();
          }
        }
        if (makeSureThatOnChangePressReleaseIsDisabled &&
            eventId == Supla::ON_CLICK_1) {
          makeSureThatOnChangePressReleaseIsDisabled = false;
          if (localHandlerForDisabledAt) {
            localHandlerForDisabledAt->disable();
          }
        }
      }
    }
    // TURN_ON/OFF publish raw edges. Only TOGGLE_x1 overrides the paired
    // local motor operation, independently of other handlers on these events.
    if (attachedButton->isBistable()) {
      for (auto handler = Supla::ActionHandlerClient::begin; handler;
           handler = handler->next) {
        if (handler->trigger == attachedButton &&
            isDirectionalHandler(handler)) {
          if (activeActionsFromServer & SUPLA_ACTION_CAP_TOGGLE_x1) {
            handler->disable();
          } else {
            handler->enable();
          }
        }
      }
    }
  }
  synchronizeDirectionalButtonModes();
}

bool Supla::Control::ActionTrigger::hasDirectionalPair() const {
  if (!attachedButton || !attachedButton->isBistable()) {
    return false;
  }
  for (auto handler = Supla::ActionHandlerClient::begin; handler;
       handler = handler->next) {
    if (handler->trigger == attachedButton && isDirectionalHandler(handler)) {
      return true;
    }
  }
  return false;
}

bool Supla::Control::ActionTrigger::requiresClickMode() const {
  return activeActionsFromServer || alwaysUseOnClick1 ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer;
}

void Supla::Control::ActionTrigger::synchronizeDirectionalButtonModes() {
  // Configuration-time scan only: no peer pointers, extra RAM or timer work.
  for (auto element = Supla::Element::begin(); element;
       element = element->next()) {
    auto at = element->getActionTrigger();
    if (!at || !at->attachedButton) {
      continue;
    }
    bool useClicks = false;
    if (at->hasDirectionalPair()) {
      useClicks = at->requiresClickMode();
      const auto related = at->channel.getRelatedChannelNumber();
      if (related && !useClicks) {
        for (auto peerElement = Supla::Element::begin(); peerElement;
             peerElement = peerElement->next()) {
          auto peer = peerElement->getActionTrigger();
          if (peer && peer->channel.getRelatedChannelNumber() == related &&
              peer->hasDirectionalPair() && peer->requiresClickMode()) {
            useClicks = true;
            break;
          }
        }
      }
    }
    at->attachedButton->setConditionalActionsOnClick1(useClicks);
  }
}

uint8_t Supla::Control::ActionTrigger::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  (void)(local);
  if (result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
      result->ConfigSize == sizeof(TChannelConfig_ActionTrigger)) {
    TChannelConfig_ActionTrigger *config =
      reinterpret_cast<TChannelConfig_ActionTrigger *>(result->Config);
    Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());
    if (channelConfigReceived &&
        lastReceivedActiveActions == config->ActiveActions) {
      // Preserve an in-flight click/release when the server repeats its
      // configuration, e.g. after reconnecting. Local callers can still force
      // a rebuild when button bindings or the channel function change.
      return SUPLA_RESULTCODE_TRUE;
    }
    lastReceivedActiveActions = config->ActiveActions;
    channelConfigReceived = true;
    activeActionsFromServer = config->ActiveActions;
    SUPLA_LOG_DEBUG(
        "AT[%d] received config with active actions: 0x%X",
        channel.getChannelNumber(),
        activeActionsFromServer);
    rebuildForAttachedButton();
    if (storageEnabled) {
      // Schedule save in 2 s after state change
      Supla::Storage::ScheduleSave(2000);
    }
  }
  return SUPLA_RESULTCODE_TRUE;
}

void Supla::Control::ActionTrigger::setRelatedChannel(Element *element) {
  if (element && element->getChannel()) {
    setRelatedChannel(element->getChannel());
  }
}

void Supla::Control::ActionTrigger::setRelatedChannel(Element &element) {
  setRelatedChannel(&element);
}

void Supla::Control::ActionTrigger::setRelatedChannel(Channel *relatedChannel) {
  if (relatedChannel) {
    channel.setRelatedChannel(relatedChannel->getChannelNumber());
    synchronizeDirectionalButtonModes();
  }
}

void Supla::Control::ActionTrigger::setRelatedChannel(Channel &relatedChannel) {
  setRelatedChannel(&relatedChannel);
}

void Supla::Control::ActionTrigger::onInit() {
  rebuildForAttachedButton();
}

void Supla::Control::ActionTrigger::rebuildForAttachedButton() {
  if (!attachedButton) {
    parseActiveActionsFromServer();
    return;
  }

  // Explicit rebuilds and changed masks discard this input's old sequence.
  // Peer synchronization alone must preserve its pending STOP/click deadline.
  if (attachedButton->conditionalActionsOnClick1) {
    attachedButton->clickCounter = 0;
    attachedButton->holdSend = 0;
  }

  if (attachedButton && localHandlerSwitchConfigured && localHandlerClient) {
    Supla::LocalAction::DeleteAction(attachedButton,
                                     localHandlerClient,
                                     Supla::ON_CLICK_1,
                                     localHandlerAction);
  }
  Supla::LocalAction::DeleteActionsHandledBy(this);
  localHandlerForEnabledAt = nullptr;
  localHandlerForDisabledAt = nullptr;
  localHandlerClient = nullptr;
  localHandlerAction = 0;
  localHandlerSwitchConfigured = false;
  disablesLocalOperation = 0;
  channel.setActionTriggerCaps(0);

  // handle automatic switch from on_press, on_release, on_change
  // events to on_click_1 for local actions on relays, roller shutters, etc.
  if (attachedButton) {
    if (attachedButton->isBistable()) {
      bool isOnChangeUsed = attachedButton->isEventAlreadyUsed(
          Supla::ON_CHANGE, false);
      bool isConditionlOnChangeUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_CHANGE, false);

      if (isOnChangeUsed != isConditionlOnChangeUsed) {
        // for bistable button use on_change <-> on_click_1
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            isOnChangeUsed ? Supla::ON_CHANGE : Supla::CONDITIONAL_ON_CHANGE);
      }
    } else if (attachedButton->isMonostable()) {
      // for monostable button use on_press/on_release <-> on_click_1
      bool isOnPressUsed =
          attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, false);
      bool isOnReleaseUsed =
          attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, false);

      bool isConditionalOnPressUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_PRESS, false);
      bool isConditionalOnReleaseUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_RELEASE, false);
      // check if only one of those bool values are set to true:
      if (isOnPressUsed && !isOnReleaseUsed && !isConditionalOnPressUsed &&
          !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt =
            attachedButton->getHandlerForFirstClient(Supla::ON_PRESS);
      } else if (isOnReleaseUsed && !isOnPressUsed &&
                 !isConditionalOnPressUsed && !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt =
            attachedButton->getHandlerForFirstClient(Supla::ON_RELEASE);
      } else if (isConditionalOnPressUsed && !isOnPressUsed &&
                 !isOnReleaseUsed && !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            Supla::CONDITIONAL_ON_PRESS);
      } else if (isConditionalOnReleaseUsed && !isOnPressUsed &&
                 !isOnReleaseUsed && !isConditionalOnPressUsed) {
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            Supla::CONDITIONAL_ON_RELEASE);
      }
    } else if (attachedButton->isMotionSensor() ||
               attachedButton->isCentral()) {
      // Nothing to do here. For motion sensor and central we always use
      // reaction to on press and on release events. Even if AT is enabled and
      // used. So no localHandlerFor* is configured here.
    }

    if (localHandlerForDisabledAt) {
      localHandlerClient = localHandlerForDisabledAt->client;
      localHandlerAction = localHandlerForDisabledAt->action;
      attachedButton->addAction(localHandlerForDisabledAt->action,
                                localHandlerForDisabledAt->client,
                                Supla::ON_CLICK_1);
      localHandlerForEnabledAt = attachedButton->getHandlerForClient(
          localHandlerForDisabledAt->client, Supla::ON_CLICK_1);
      if (localHandlerForEnabledAt) {
        localHandlerForEnabledAt->disable();
        localHandlerSwitchConfigured = true;
      } else {
        localHandlerClient = nullptr;
        localHandlerAction = 0;
      }
    }
  }

  if (attachedButton) {
    // Configure default actions for bistable button
    if (attachedButton->isBistable()) {
      for (auto handler = Supla::ActionHandlerClient::begin; handler;
           handler = handler->next) {
        if (handler->trigger != attachedButton || handler->isAlwaysEnabled()) {
          continue;
        }
        if (isDirectionalHandler(handler)) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
        } else if (handler->onEvent == Supla::ON_PRESS ||
                   handler->onEvent == Supla::CONDITIONAL_ON_PRESS) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
        } else if (handler->onEvent == Supla::ON_RELEASE ||
                   handler->onEvent == Supla::CONDITIONAL_ON_RELEASE) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
        }
      }
      // Bistable roller shutters use conditional press/release for one
      // directional local action pair. Preserve the old bistable behavior:
      // TOGGLE_x1 must disable both local edges as one local operation.
      if (attachedButton->isEventAlreadyUsed(
              Supla::CONDITIONAL_ON_PRESS, true) &&
          attachedButton->isEventAlreadyUsed(
              Supla::CONDITIONAL_ON_RELEASE, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1, true) ||
          attachedButton->isEventAlreadyUsed(Supla::ON_CHANGE, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_CHANGE,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x2;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x3;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x4;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x5;
      }

      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x1)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x1,
                                      Supla::ON_CLICK_1);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x2)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x2,
                                      Supla::ON_CLICK_2);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x3)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x3,
                                      Supla::ON_CLICK_3);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x4)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x4,
                                      Supla::ON_CLICK_4);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x5)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x5,
                                      Supla::ON_CLICK_5);
      }

    } else if (attachedButton->isMonostable()) {
      // Configure default actions for monostable button
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_HOLD, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_HOLD;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x2;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x3;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x4;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x5;
      }

      if (!(disabledCapabilities & SUPLA_ACTION_CAP_HOLD)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_HOLD, Supla::ON_HOLD);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x1)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x1,
                                      Supla::ON_CLICK_1);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x2)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x2,
                                      Supla::ON_CLICK_2);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x3)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x3,
                                      Supla::ON_CLICK_3);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x4)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x4,
                                      Supla::ON_CLICK_4);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x5)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x5,
                                      Supla::ON_CLICK_5);
      }

    } else if (attachedButton->isMotionSensor() ||
               attachedButton->isCentral()) {
      // Configure default actions for motion sensor and central buttons
      if (attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
    }
  }

  channel.setDisablesLocalOperation(disablesLocalOperation);
  parseActiveActionsFromServer();
}

void Supla::Control::ActionTrigger::disableATCapability(uint32_t capToDisable) {
  disabledCapabilities |= capToDisable;
}

void Supla::Control::ActionTrigger::onSaveState() {
  if (storageEnabled) {
    Supla::Storage::WriteState(
        reinterpret_cast<unsigned char *>(&activeActionsFromServer),
                             sizeof(activeActionsFromServer));
  }
}
void Supla::Control::ActionTrigger::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg == nullptr) {
    return;
  }

  int32_t value = 0;  // default value
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key,
                             getChannel()->getChannelNumber(),
                             Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);
  cfg->getInt32(key, &value);

  const auto previousHandlingType = actionHandlingType;
  switch (value) {
    case 0:
    default: {
      // rely on configuration from Supla server
      // In MQTT mode only actions enabled on Supla server are published on
      // MQTT.
      actionHandlingType = ActionHandlingType_RelayOnSuplaServer;
      break;
    }
    case 1: {
      // All events will be published to Supla server and/or MQTT.
      // Local actions are disabled only if it is done on Supla server
      actionHandlingType = ActionHandlingType_PublishAllDisableNone;
      break;
    }
    case 2: {
      // disable local input regardless of Supla server config
      // All events are published
      actionHandlingType = ActionHandlingType_PublishAllDisableAll;
      break;
    }
  }
  if (previousHandlingType != actionHandlingType) {
    channelConfigReceived = false;
  }
}

void Supla::Control::ActionTrigger::onLoadState() {
  if (storageEnabled) {
    channelConfigReceived = false;
    Supla::Storage::ReadState((unsigned char *)&activeActionsFromServer,
        sizeof(activeActionsFromServer));
    if (activeActionsFromServer) {
      SUPLA_LOG_INFO(
          "AT[%d]: restored activeActionsFromServer: 0x%X",
          channel.getChannelNumber(),
          activeActionsFromServer);
    }
  }
}

void Supla::Control::ActionTrigger::enableStateStorage() {
  storageEnabled = true;
}

void Supla::Control::ActionTrigger::addActionToButtonAndDisableIt(int action,
                                                                  int event) {
  attachedButton->addAction(action, this, event);
  attachedButton->disableAction(action, this, event);
}

bool Supla::Control::ActionTrigger::isAnyActionEnabledOnServer() const {
  return activeActionsFromServer != 0;
}

void Supla::Control::ActionTrigger::setAlwaysUseOnClick1() {
  alwaysUseOnClick1 = true;
  synchronizeDirectionalButtonModes();
}

void Supla::Control::ActionTrigger::enable() {
  enabled = true;
}

void Supla::Control::ActionTrigger::disable() {
  enabled = false;
}
