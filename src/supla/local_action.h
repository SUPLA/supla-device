// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_LOCAL_ACTION_H_
#define SRC_SUPLA_LOCAL_ACTION_H_

#include <stdint.h>

namespace Supla {

// Fixed-size action list used by the local-action filter.  This deliberately
// avoids std::initializer_list because the AVR Arduino toolchain does not
// provide that header.  The current firmware uses at most three entries for a
// single event (the selected lock action and the two config-mode actions).
class ActionAllowList {
 public:
  ActionAllowList() : count(0) {
    actions[0] = 0;
    actions[1] = 0;
    actions[2] = 0;
  }

  ActionAllowList(uint16_t action) : count(1) {  // NOLINT(runtime/explicit)
    actions[0] = action;
    actions[1] = 0;
    actions[2] = 0;
  }

  ActionAllowList(uint16_t action1, uint16_t action2) : count(2) {
    actions[0] = action1;
    actions[1] = action2;
    actions[2] = 0;
  }

  ActionAllowList(uint16_t action1, uint16_t action2, uint16_t action3)
      : count(3) {
    actions[0] = action1;
    actions[1] = action2;
    actions[2] = action3;
  }

  bool contains(uint16_t action) const {
    for (uint8_t i = 0; i < count; i++) {
      if (actions[i] == action) {
        return true;
      }
    }
    return false;
  }

 private:
  uint16_t actions[3];
  uint8_t count;
};

class ActionHandler;
class LocalAction;

class ActionHandlerClient {
 public:
  ActionHandlerClient();

  virtual ~ActionHandlerClient();

  LocalAction *trigger = nullptr;
  ActionHandler *client = nullptr;
  ActionHandlerClient *next = nullptr;
  uint16_t onEvent = 0;
  uint16_t action = 0;
  static ActionHandlerClient *begin;

  bool isEnabled();

  virtual void setAlwaysEnabled();
  virtual void enable();
  virtual void disable();
  void disableForConfigMode();
  void restoreAfterConfigMode();
  virtual bool isAlwaysEnabled();

 protected:
  bool enabled = true;
  bool alwaysEnabled = false;
  bool disabledForConfigMode = false;
};

class LocalAction {
 public:
  virtual ~LocalAction();
  virtual void addAction(uint16_t action,
      ActionHandler &client,   // NOLINT(runtime/references)
      uint16_t event,
      bool alwaysEnabled = false);
  virtual void addAction(uint16_t action, ActionHandler *client, uint16_t event,
      bool alwaysEnabled = false);

  virtual void runAction(uint16_t event) const;
  // The allow-list is an additional restriction.  An empty list deliberately
  // blocks every action, while the one-argument overload keeps the historic,
  // unrestricted behavior.
  virtual void runAction(
      uint16_t event,
      const ActionAllowList &allowOnlyActions) const;

  virtual bool isEventAlreadyUsed(uint16_t event, bool ignoreAlwaysEnabled);
  virtual bool hasEnabledAction(uint16_t event, uint16_t action) const;
  virtual ActionHandlerClient *getHandlerForFirstClient(uint16_t event);
  virtual ActionHandlerClient *getHandlerForClient(ActionHandler *client,
                                                   uint16_t event);

  virtual void disableOtherClients(const ActionHandler &client, uint16_t event);
  virtual void enableOtherClients(const ActionHandler &client, uint16_t event);
  virtual void disableOtherClients(const ActionHandler *client, uint16_t event);
  virtual void enableOtherClients(const ActionHandler *client, uint16_t event);

  static void DeleteActionsHandledBy(const ActionHandler *client);
  static void DeleteActionsHandledByExcept(
      const ActionHandler *client,
      const ActionAllowList &preservedActions);
  static void DeleteActionsTriggeredBy(const LocalAction *action);
  static void DeleteAction(const LocalAction *trigger,
                           const ActionHandler *client,
                           uint16_t event,
                           uint16_t action);
  static void NullifyActionsHandledBy(const ActionHandler *client);

  // action and event are internally uint16_t type, however -1 is used
  // as "all events/actions", so here we pass int32_t
  virtual void disableAction(int32_t action,
                             ActionHandler *client,
                             int32_t event);
  virtual void enableAction(int32_t action,
                            ActionHandler *client,
                            int32_t event);

  virtual bool disableActionsInConfigMode();

  static ActionHandlerClient *getClientListPtr();

 private:
  void runActionInternal(uint16_t event,
                         const ActionAllowList *allowOnlyActions) const;
};

};  // namespace Supla

#endif  // SRC_SUPLA_LOCAL_ACTION_H_
