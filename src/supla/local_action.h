// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_LOCAL_ACTION_H_
#define SRC_SUPLA_LOCAL_ACTION_H_

#include <stdint.h>

namespace Supla {

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

  virtual bool isEventAlreadyUsed(uint16_t event, bool ignoreAlwaysEnabled);
  virtual ActionHandlerClient *getHandlerForFirstClient(uint16_t event);
  virtual ActionHandlerClient *getHandlerForClient(ActionHandler *client,
                                                   uint16_t event);

  virtual void disableOtherClients(const ActionHandler &client, uint16_t event);
  virtual void enableOtherClients(const ActionHandler &client, uint16_t event);
  virtual void disableOtherClients(const ActionHandler *client, uint16_t event);
  virtual void enableOtherClients(const ActionHandler *client, uint16_t event);

  static void DeleteActionsHandledBy(const ActionHandler *client);
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
};

};  // namespace Supla

#endif  // SRC_SUPLA_LOCAL_ACTION_H_
