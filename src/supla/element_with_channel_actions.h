/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
#define SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_

#include <supla/element.h>
#include <supla/channels/channel.h>
#include <supla/local_action.h>
#include <supla/action_handler.h>
#include <supla/condition.h>

namespace Supla {

enum class ChannelConfigState : uint8_t {
  None = 0,
  LocalChangePending = 1,
  SetChannelConfigSend = 2,
  SetChannelConfigFailed = 3,
  WaitForConfigFinished = 4
};

class Condition;

class ElementWithChannelActions : public Element, public LocalAction {
 public:
  // Override local action methods in order to delegate execution to Channel
  void addAction(uint16_t action,
      ActionHandler &client,  // NOLINT(runtime/references)
      uint16_t event,
      bool alwaysEnabled = false) override;
  void addAction(uint16_t action, ActionHandler *client, uint16_t event,
      bool alwaysEnabled = false) override;
  virtual void addAction(uint16_t action,
      ActionHandler &client,  // NOLINT(runtime/references)
      Supla::Condition *condition,
      bool alwaysEnabled = false);
  virtual void addAction(uint16_t action, ActionHandler *client,
      Supla::Condition *condition,
      bool alwaysEnabled = false);

  bool isEventAlreadyUsed(uint16_t event, bool ignoreAlwaysEnabled) override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  bool iterateConnected() override;
  void handleChannelConfigFinished() override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *result, bool local) override;
  virtual uint8_t applyChannelConfig(TSD_ChannelConfig *result);
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;

  void clearChannelConfigChangedFlag();

  void runAction(uint16_t event) override;

  // returns true if function was changed (previous one was different)
  virtual bool setAndSaveFunction(_supla_int_t channelFunction);
  virtual bool loadFunctionFromConfig();
  virtual bool saveConfigChangeFlag();
  virtual bool loadConfigChangeFlag();
  virtual void fillChannelConfig(void *channelConfig, int *size);
  bool isAnyUpdatePending() override;

 protected:
  Supla::ChannelConfigState channelConfigState =
      Supla::ChannelConfigState::None;
  bool configFinishedReceived = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
