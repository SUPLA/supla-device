// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "local_action.h"

#include <supla/action_handler.h>
#include <supla/actions.h>

namespace Supla {
namespace {
uint16_t actionDispatchDepth = 0;
void retire(ActionHandlerClient *entry) {
  if (actionDispatchDepth) {
    entry->deletePending = true;
    entry->disable();
  } else {
    delete entry;
  }
}
void collectRetired() {
  auto entry = ActionHandlerClient::begin;
  while (entry) {
    if (entry->deletePending) {
      delete entry;
      entry = ActionHandlerClient::begin;
    } else {
      entry = entry->next;
    }
  }
}
struct DispatchFrame;
DispatchFrame *activeDispatch = nullptr;
struct DispatchFrame {
  explicit DispatchFrame(const LocalAction *source)
      : trigger(source), previous(activeDispatch) {
    activeDispatch = this;
    ++actionDispatchDepth;
  }
  ~DispatchFrame() {
    activeDispatch = previous;
    if (--actionDispatchDepth == 0) collectRetired();
  }
  const LocalAction *trigger;
  DispatchFrame *previous;
  bool canceled = false;
};
}  // namespace


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
  if (client && client->deleteClient()) {
    delete client;
    client = nullptr;
  }

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
  return enabled && !deletePending;
}

void ActionHandlerClient::enable() {
  enabled = true;
  disabledForConfigMode = false;
}

void ActionHandlerClient::disable() {
  if (!alwaysEnabled) {
    enabled = false;
    disabledForConfigMode = false;
  }
}

void ActionHandlerClient::disableForConfigMode() {
  if (!alwaysEnabled && enabled) {
    enabled = false;
    disabledForConfigMode = true;
  }
}

void ActionHandlerClient::restoreAfterConfigMode() {
  if (disabledForConfigMode) {
    enabled = true;
    disabledForConfigMode = false;
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
  DeleteActionsTriggeredBy(this);
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

void LocalAction::baselineActions() const {
  DispatchFrame dispatch(this);
  for (auto entry = ActionHandlerClient::begin; entry && !dispatch.canceled;
       entry = entry->next) {
    if (entry->trigger == this && entry->client && entry->isEnabled())
      entry->client->baselineState();
  }
}

void LocalAction::runAction(uint16_t event) const {
  runActionInternal(event, nullptr);
}

void LocalAction::runAction(
    uint16_t event, const ActionAllowList &allowOnlyActions) const {
  runActionInternal(event, &allowOnlyActions);
}

void LocalAction::runActionInternal(
    uint16_t event, const ActionAllowList *allowOnlyActions) const {
  DispatchFrame dispatch(this);
  auto ptr = ActionHandlerClient::begin;
  while (ptr && !dispatch.canceled) {
    bool actionAllowed = allowOnlyActions == nullptr ||
                         allowOnlyActions->contains(ptr->action);
    bool duplicateLockAction = false;
    if (allowOnlyActions != nullptr && actionAllowed && ptr->client &&
        (ptr->action == Supla::LOCK || ptr->action == Supla::UNLOCK ||
         ptr->action == Supla::TOGGLE_LOCK)) {
      auto previous = ActionHandlerClient::begin;
      while (previous != ptr) {
        if (previous->client == ptr->client &&
            previous->trigger == this && previous->onEvent == event &&
            previous->action == ptr->action && previous->isEnabled()) {
          duplicateLockAction = true;
          break;
        }
        previous = previous->next;
      }
    }
    if (ptr->client && ptr->trigger == this && ptr->onEvent == event &&
        ptr->isEnabled() && actionAllowed && !duplicateLockAction) {
      ptr->client->handleAction(event, ptr->action);
    }
    ptr = ptr->next;
  }
}

bool LocalAction::hasEnabledAction(uint16_t event, uint16_t action) const {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    if (ptr->client && ptr->trigger == this && ptr->onEvent == event &&
        ptr->action == action && ptr->isEnabled()) {
      return true;
    }
    ptr = ptr->next;
  }
  return false;
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
    actionToCheck = static_cast<uint16_t>(action);
  }
  if (event >= 0 && event <= 65535) {
    eventToCheck = static_cast<uint16_t>(event);
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
    actionToCheck = static_cast<uint16_t>(action);
  }
  if (event >= 0 && event <= 65535) {
    eventToCheck = static_cast<uint16_t>(event);
  }
  while (ptr) {
    if (ptr->trigger == this && (ptr->onEvent == eventToCheck || allEvents) &&
        ptr->client == client && (ptr->action == actionToCheck || allActions)) {
      ptr->enable();
    }
    ptr = ptr->next;
  }
}

void LocalAction::DeleteActionsHandledBy(const ActionHandler *client) {
  DeleteActionsHandledByExcept(client, {});
}

void LocalAction::DeleteActionsHandledByExcept(
    const ActionHandler *client,
    const ActionAllowList &preservedActions) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    auto next = ptr->next;
    bool preserve = false;
    preserve = preservedActions.contains(ptr->action);
    if (!ptr->deletePending && !preserve && ptr->client &&
        ptr->client->getRealClient() == client) {
      retire(ptr);
      next = ActionHandlerClient::begin;
    }
    ptr = next;
  }
}

void LocalAction::CancelDispatch(const LocalAction *trigger) {
  for (auto frame = activeDispatch; frame; frame = frame->previous) {
    if (frame->trigger == trigger) frame->canceled = true;
  }
}
void LocalAction::DeleteActionsTriggeredBy(const LocalAction *trigger) {
  CancelDispatch(trigger);
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    auto next = ptr->next;
    if (!ptr->deletePending && ptr->trigger == trigger) {
      retire(ptr);
      next = ActionHandlerClient::begin;
    }
    ptr = next;
  }
}

void LocalAction::DeleteActionsBetween(const LocalAction *trigger,
                                       const ActionHandler *client) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    auto next = ptr->next;
    if (!ptr->deletePending && ptr->trigger == trigger && ptr->client &&
        ptr->client->getRealClient() == client) {
      retire(ptr);
      next = ActionHandlerClient::begin;
    }
    ptr = next;
  }
}

void LocalAction::DeleteAction(const LocalAction *trigger,
                               const ActionHandler *client,
                               uint16_t event,
                               uint16_t action) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    auto next = ptr->next;
    if (!ptr->deletePending && ptr->trigger == trigger &&
        ptr->client == client &&
        ptr->onEvent == event && ptr->action == action) {
      retire(ptr);
      next = ActionHandlerClient::begin;
    }
    ptr = next;
  }
}

void LocalAction::NullifyActionsHandledBy(const ActionHandler *client) {
  auto ptr = ActionHandlerClient::begin;
  while (ptr) {
    auto next = ptr->next;
    if (!ptr->deletePending && ptr->client &&
        ptr->client->getRealClient() == client) {
      if (ptr->client != client && ptr->client->deleteClient()) {
        if (!ptr->deletePending) retire(ptr);
        next = ActionHandlerClient::begin;
      } else {
        ptr->client = nullptr;
      }
    }
    ptr = next;
  }
}
}  // namespace Supla
