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

#ifndef SRC_SUPLA_LOCAL_ACTION_H_
#define SRC_SUPLA_LOCAL_ACTION_H_

#include <stdint.h>
#include "action_handler.h"

namespace Supla {

class LocalAction;
class Condition;

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
  virtual bool isAlwaysEnabled();

 protected:
  bool enabled = true;
  bool alwaysEnabled = false;
};

class LocalAction {
 public:
  virtual ~LocalAction();
  virtual void addAction(int action,
      ActionHandler &client,   // NOLINT(runtime/references)
      int event,
      bool alwaysEnabled = false);
  virtual void addAction(int action, ActionHandler *client, int event,
      bool alwaysEnabled = false);

  virtual void runAction(int event);

  virtual bool isEventAlreadyUsed(int event);
  virtual ActionHandlerClient *getHandlerForFirstClient(int event);
  virtual ActionHandlerClient *getHandlerForClient(ActionHandler *client,
                                                   int event);

  virtual void disableOtherClients(const ActionHandler &client, int event);
  virtual void enableOtherClients(const ActionHandler &client, int event);
  virtual void disableOtherClients(const ActionHandler *client, int event);
  virtual void enableOtherClients(const ActionHandler *client, int event);

  virtual void disableAction(int action, ActionHandler *client, int event);
  virtual void enableAction(int action, ActionHandler *client, int event);

  virtual bool disableActionsInConfigMode();

  static ActionHandlerClient *getClientListPtr();
};

};  // namespace Supla

#endif  // SRC_SUPLA_LOCAL_ACTION_H_
