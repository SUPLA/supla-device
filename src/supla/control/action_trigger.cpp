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

#include "action_trigger.h"

#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/control/button.h>
#include <stdio.h>
#include <supla/events.h>
#include <supla/network/html/button_action_trigger_config.h>

Supla::Control::ActionTrigger::ActionTrigger() {
  channel.setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  channel.setDefault(SUPLA_CHANNELFNC_ACTIONTRIGGER);
}

Supla::Control::ActionTrigger::~ActionTrigger() {
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button *button) {
  attachedButton = button;
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button &button) {
  attach(&button);
}

void Supla::Control::ActionTrigger::handleAction(int event, int action) {
  (void)(event);
  uint32_t actionCap = getActionTriggerCap(action);

  if (actionCap & activeActionsFromServer ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer) {
    channel.pushAction(actionCap);
  }
}

Supla::Channel *Supla::Control::ActionTrigger::getChannel() {
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

  channel.requestChannelConfig();
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
        actionHandlingType == ActionHandlingType_PublishAllDisableNone) {
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
        attachedButton->disableAction(actionId, this, eventId);
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
        }
        if (makeSureThatOnClick1IsDisabled && eventId == Supla::ON_CLICK_1) {
          makeSureThatOnClick1IsDisabled = false;
          localHandlerForEnabledAt->disable();
        }
        if (makeSureThatOnChangePressReleaseIsDisabled &&
            eventId == Supla::ON_CLICK_1) {
          makeSureThatOnChangePressReleaseIsDisabled = false;
          localHandlerForDisabledAt->disable();
        }
      }
    }
  }
}

uint8_t Supla::Control::ActionTrigger::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  (void)(local);
  if (result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
      result->ConfigSize == sizeof(TChannelConfig_ActionTrigger)) {
    TChannelConfig_ActionTrigger *config =
      reinterpret_cast<TChannelConfig_ActionTrigger *>(result->Config);
    activeActionsFromServer = config->ActiveActions;
    SUPLA_LOG_DEBUG(
        "AT[%d] received config with active actions: 0x%X",
        channel.getChannelNumber(),
        activeActionsFromServer);
    parseActiveActionsFromServer();
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
  }
}

void Supla::Control::ActionTrigger::setRelatedChannel(Channel &relatedChannel) {
  setRelatedChannel(&relatedChannel);
}

void Supla::Control::ActionTrigger::onInit() {
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
    } else if (attachedButton->isMotionSensor()) {
      // Nothing to do here. For motion sensor we always use reaction to
      // on press and on release events. Even if AT is enabled and used.
      // So no localHandlerFor* is configured here.
    }

    if (localHandlerForDisabledAt) {
      attachedButton->addAction(localHandlerForDisabledAt->action,
                                localHandlerForDisabledAt->client,
                                Supla::ON_CLICK_1);
      localHandlerForEnabledAt = attachedButton->getHandlerForClient(
          localHandlerForDisabledAt->client, Supla::ON_CLICK_1);
      localHandlerForEnabledAt->disable();
    }
  }

  if (attachedButton) {
    // Configure default actions for bistable button
    if (attachedButton->isBistable()) {
      if (attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_RELEASE,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
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

    } else if (attachedButton->isMotionSensor()) {
      // Configure default actions for motion sensor button
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
                             Supla::Html::BtnActionTriggerCfgTagPrefix);
  cfg->getInt32(key, &value);

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
}

void Supla::Control::ActionTrigger::onLoadState() {
  if (storageEnabled) {
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
