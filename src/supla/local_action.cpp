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

#include "local_action.h"

namespace Supla {

ActionHandlerClient::ActionHandlerClient() {
  if (begin == nullptr) {
    begin = this;
  } else {
    auto ptr = begin;
    while (ptr->next) {
      ptr = ptr->next;
    }
    ptr->next = this;
  }
}

ActionHandlerClient::~ActionHandlerClient() {
  if (begin == this) {
    begin = next;
    return;
  }

  auto ptr = begin;
  while (ptr->next != this) {
    ptr = ptr->next;
  }

  ptr->next = ptr->next->next;
}

bool ActionHandlerClient::isEnabled() {
  return enabled;
}

void ActionHandlerClient::enable() {
  enabled = true;
}

void ActionHandlerClient::disable() {
  if (!alwaysEnabled) {
    enabled = false;
  }
}

void ActionHandlerClient::setAlwaysEnabled() {
  alwaysEnabled = true;
  enabled = true;
}

bool ActionHandlerClient::isAlwaysEnabled() {
  return alwaysEnabled;
}

ActionHandlerClient *ActionHandlerClient::begin = nullptr;

LocalAction::~LocalAction() {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this) {
      auto tbdptr = ptr;
      ptr = ptr->next;
      if (tbdptr->client->deleteClient()) {
        delete tbdptr->client;
      }
      delete tbdptr;
    } else {
      ptr = ptr->next;
    }
  }
}

void LocalAction::addAction(uint16_t action,
                            ActionHandler &client,
                            uint16_t event,
                            bool alwaysEnabled) {
  auto ptr = new ActionHandlerClient;
  ptr->trigger = this;
  ptr->client = &client;
  ptr->onEvent = event;
  ptr->action = action;
  ptr->client->activateAction(action);
  if (alwaysEnabled) {
    ptr->setAlwaysEnabled();
  }
}

void LocalAction::addAction(uint16_t action,
                            ActionHandler *client,
                            uint16_t event,
                            bool alwaysEnabled) {
  LocalAction::addAction(action, *client, event, alwaysEnabled);
}

void LocalAction::runAction(uint16_t event) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->onEvent == event && ptr->isEnabled()) {
      ptr->client->handleAction(event, ptr->action);
    }
    ptr = ptr->next;
  }
}

ActionHandlerClient *LocalAction::getClientListPtr() {
  return ActionHandlerClient::begin;
}

bool LocalAction::isEventAlreadyUsed(uint16_t event, bool ignoreAlwaysEnabled) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->onEvent == event &&
        (!ignoreAlwaysEnabled || !ptr->isAlwaysEnabled())) {
      return true;
    }
    ptr = ptr->next;
  }
  return false;
}

void LocalAction::disableOtherClients(const ActionHandler &client,
                                      uint16_t event) {
  disableOtherClients(&client, event);
}

void LocalAction::enableOtherClients(const ActionHandler &client,
                                     uint16_t event) {
  enableOtherClients(&client, event);
}

void LocalAction::disableOtherClients(const ActionHandler *client,
                                      uint16_t event) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->onEvent == event &&
        ptr->client != client) {
      ptr->disable();
    }
    ptr = ptr->next;
  }
}

void LocalAction::enableOtherClients(const ActionHandler *client,
                                     uint16_t event) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->onEvent == event &&
        ptr->client != client) {
      ptr->enable();
    }
    ptr = ptr->next;
  }
}

ActionHandlerClient *LocalAction::getHandlerForFirstClient(uint16_t event) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->onEvent == event) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return nullptr;
}

ActionHandlerClient *LocalAction::getHandlerForClient(ActionHandler *client,
                                                   uint16_t event) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->trigger == this && ptr->client == client
        && ptr->onEvent == event) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return nullptr;
}

bool LocalAction::disableActionsInConfigMode() {
  return false;
}

void LocalAction::disableAction(int32_t action,
                                ActionHandler *client,
                                int32_t event) {
  auto ptr = ActionHandlerClient::begin;
  bool allEvents = (event == -1);
  bool allActions = (action == -1);
  uint16_t eventToCheck = 0;
  uint16_t actionToCheck = 0;
  if (action >= 0 && action <= 65535) {
    actionToCheck = action;
  }
  if (event >= 0 && event <= 65535) {
    eventToCheck = event;
  }

  while (ptr) {
    if (ptr->trigger == this && (ptr->onEvent == eventToCheck || allEvents) &&
        ptr->client == client && (ptr->action == actionToCheck || allActions)) {
      ptr->disable();
    }
    ptr = ptr->next;
  }
}

void LocalAction::enableAction(int32_t action,
                               ActionHandler *client,
                               int32_t event) {
  auto ptr = ActionHandlerClient::begin;
  bool allEvents = (event == -1);
  bool allActions = (action == -1);
  uint16_t eventToCheck = 0;
  uint16_t actionToCheck = 0;
  if (action >= 0 && action <= 65535) {
    actionToCheck = action;
  }
  if (event >= 0 && event <= 65535) {
    eventToCheck = event;
  }
  while (ptr) {
    if (ptr->trigger == this && (ptr->onEvent == eventToCheck || allEvents) &&
        ptr->client == client && (ptr->action == actionToCheck || allActions)) {
      ptr->enable();
    }
    ptr = ptr->next;
  }
}

};  // namespace Supla
