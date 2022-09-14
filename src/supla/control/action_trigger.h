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

#ifndef SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_
#define SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../actions.h"
#include "../at_channel.h"
#include "../element.h"
#include "button.h"
#include "supla/protocol/supla_srpc.h"

namespace Supla {
namespace Control {

class ActionTrigger : public Element, public ActionHandler {
 public:
  ActionTrigger();
  virtual ~ActionTrigger();

  // Use below methods to attach button instance to ActionTrigger.
  // It will automatically register to all supported button actions
  // during onInit() call on action trigger instance.
  void attach(Supla::Control::Button *);
  void attach(Supla::Control::Button &);

  // Makes AT channel related to other channel, so Supla Cloud will not
  // list AT as a separate channel, but it will be extending i.e. Relay
  // channel.
  void setRelatedChannel(Element *);
  void setRelatedChannel(Channel *);
  void setRelatedChannel(Element &);
  void setRelatedChannel(Channel &);

  void handleAction(int event, int action) override;
  void activateAction(int action) override;
  Supla::Channel *getChannel() override;
  void onInit() override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr) override;
  void handleChannelConfig(TSD_ChannelConfig *result) override;
  void onLoadState() override;
  void onSaveState() override;

  void disableATCapability(uint32_t capToDisable);
  void enableStateStorage();

  static int actionTriggerCapToButtonEvent(uint32_t actionCap);
  static int getActionTriggerCap(int action);

 protected:
  void parseActiveActionsFromServer();
  Supla::AtChannel channel;
  Supla::Control::Button *attachedButton = nullptr;
  uint32_t activeActionsFromServer = 0;
  uint32_t disablesLocalOperation = 0;
  uint32_t disabledCapabilities = 0;
  bool storageEnabled = false;

  Supla::ActionHandlerClient *localHandlerForEnabledAt = nullptr;
  Supla::ActionHandlerClient *localHandlerForDisabledAt = nullptr;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_
