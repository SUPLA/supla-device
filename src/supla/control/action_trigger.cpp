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

#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <stdio.h>

#include "action_trigger.h"


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
  return 0;
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

    if (activeActionsFromServer ||
        actionHandlingType == ActionHandlingType_PublishAllDisableNone) {
      // disable on_press, on_release, on_change local actions and enable
      // on_click_1
      if (localHandlerForDisabledAt && localHandlerForEnabledAt) {
        localHandlerForDisabledAt->disable();
        localHandlerForEnabledAt->enable();
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

    for (int i = 0; i < 32; i++) {
      uint32_t actionCap = (1 << i);
      if (actionsToDisable & actionCap) {
        int eventToDisable = actionTriggerCapToButtonEvent(actionCap);
        attachedButton->disableOtherClients(this, eventToDisable);
      } else if (disablesLocalOperation & actionCap) {
        int eventToEnable = actionTriggerCapToButtonEvent(actionCap);
        attachedButton->enableOtherClients(this, eventToEnable);
        if (makeSureThatOnClick1IsDisabled &&
            eventToEnable == Supla::ON_CLICK_1) {
          makeSureThatOnClick1IsDisabled = false;
          localHandlerForEnabledAt->disable();
        }
      }
    }
  }
}

uint8_t Supla::Control::ActionTrigger::handleChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  (void)(local);
  if (result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT &&
      result->ConfigSize == sizeof(TSD_ChannelConfig_ActionTrigger)) {
    TSD_ChannelConfig_ActionTrigger *config =
      reinterpret_cast<TSD_ChannelConfig_ActionTrigger *>(result->Config);
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
    if (attachedButton->isBistable() &&
        attachedButton->isEventAlreadyUsed(Supla::ON_CHANGE)) {
      // for bistable button use on_change <-> on_click_1
      localHandlerForDisabledAt =
          attachedButton->getHandlerForFirstClient(Supla::ON_CHANGE);
    }
    if (!attachedButton->isBistable()) {
      // for monostable button use on_press/on_release <-> on_click_1
      bool onPress = attachedButton->isEventAlreadyUsed(Supla::ON_PRESS);
      bool onRelease = attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE);

      if (onPress != onRelease) {
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            onPress ? Supla::ON_PRESS : Supla::ON_RELEASE);
      }
    }

    if (localHandlerForDisabledAt) {
      attachedButton->addAction(localHandlerForDisabledAt->action,
                                localHandlerForDisabledAt->client,
                                Supla::ON_CLICK_1);
      localHandlerForEnabledAt =
          attachedButton->getHandlerForClient(
              localHandlerForDisabledAt->client, Supla::ON_CLICK_1);
      localHandlerForEnabledAt->disable();
    }
  }

  // Configure default actions for bistable button
  if (attachedButton && attachedButton->isBistable()) {
    if (attachedButton->isEventAlreadyUsed(Supla::ON_PRESS)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x2;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x3;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x4;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x5;
    }

    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
      attachedButton->addAction(Supla::SEND_AT_TURN_ON, this, Supla::ON_PRESS);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
      attachedButton->addAction(
          Supla::SEND_AT_TURN_OFF, this, Supla::ON_RELEASE);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x1)) {
      attachedButton->addAction(
          Supla::SEND_AT_TOGGLE_x1, this, Supla::ON_CLICK_1);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x2)) {
      attachedButton->addAction(
          Supla::SEND_AT_TOGGLE_x2, this, Supla::ON_CLICK_2);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x3)) {
      attachedButton->addAction(
          Supla::SEND_AT_TOGGLE_x3, this, Supla::ON_CLICK_3);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x4)) {
      attachedButton->addAction(
          Supla::SEND_AT_TOGGLE_x4, this, Supla::ON_CLICK_4);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x5)) {
      attachedButton->addAction(
          Supla::SEND_AT_TOGGLE_x5, this, Supla::ON_CLICK_5);
    }
  }

  // Configure default actions for monostable button
  if (attachedButton && !attachedButton->isBistable()) {
    if (attachedButton->isEventAlreadyUsed(Supla::ON_HOLD)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_HOLD;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x1;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x2;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x3;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x4;
    }
    if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5)) {
      disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x5;
    }

    if (!(disabledCapabilities & SUPLA_ACTION_CAP_HOLD)) {
      attachedButton->addAction(Supla::SEND_AT_HOLD, this, Supla::ON_HOLD);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x1)) {
      attachedButton->addAction(
          Supla::SEND_AT_SHORT_PRESS_x1, this, Supla::ON_CLICK_1);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x2)) {
      attachedButton->addAction(
          Supla::SEND_AT_SHORT_PRESS_x2, this, Supla::ON_CLICK_2);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x3)) {
      attachedButton->addAction(
          Supla::SEND_AT_SHORT_PRESS_x3, this, Supla::ON_CLICK_3);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x4)) {
      attachedButton->addAction(
          Supla::SEND_AT_SHORT_PRESS_x4, this, Supla::ON_CLICK_4);
    }
    if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x5)) {
      attachedButton->addAction(
          Supla::SEND_AT_SHORT_PRESS_x5, this, Supla::ON_CLICK_5);
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
void Supla::Control::ActionTrigger::onLoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();

  if (cfg == nullptr) {
    return;
  }

  int32_t value = 0;  // default value
  char key[16] = {};
  snprintf(key, sizeof(key), "mqtt_at_%d", getChannelNumber());
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
