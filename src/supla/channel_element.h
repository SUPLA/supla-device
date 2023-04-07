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

#ifndef SRC_SUPLA_CHANNEL_ELEMENT_H_
#define SRC_SUPLA_CHANNEL_ELEMENT_H_

#include "element.h"
#include "channel.h"
#include "local_action.h"
#include "action_handler.h"
#include "condition.h"

namespace Supla {

class Condition;

class ChannelElement : public Element, public LocalAction {
 public:
  Channel *getChannel() override;

  // Override local action methods in order to delegate execution to Channel
  void addAction(int action,
      ActionHandler &client,  // NOLINT(runtime/references)
      int event,
      bool alwaysEnabled = false) override;
  void addAction(int action, ActionHandler *client, int event,
      bool alwaysEnabled = false) override;
  virtual void addAction(int action,
      ActionHandler &client,  // NOLINT(runtime/references)
      Supla::Condition *condition,
      bool alwaysEnabled = false);
  virtual void addAction(int action, ActionHandler *client,
      Supla::Condition *condition,
      bool alwaysEnabled = false);

  bool isEventAlreadyUsed(int event) override;

  void runAction(int event) override;

  virtual bool loadFunctionFromConfig();
  // returns true if function was changed (previous one was different)
  virtual bool setAndSaveFunction(_supla_int_t channelFunction);

 protected:
  Channel channel;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CHANNEL_ELEMENT_H_
