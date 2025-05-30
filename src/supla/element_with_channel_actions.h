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

#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/element.h>
#include <supla/local_action.h>

namespace Supla {

enum class ChannelConfigState : uint8_t {
  None = 0,
  LocalChangePending = 1,
  SetChannelConfigSend = 2,
  SetChannelConfigFailed = 3,
  WaitForConfigFinished = 4,
  ResendConfig = 5,
  LocalChangeSent = 6
};

enum class ApplyConfigResult : uint8_t {
  NotSupported,
  Success,
  DataError,
  SetChannelConfigNeeded,
};

#pragma pack(push, 1)
struct ConfigTypesBitmap {
 private:
  union {
    struct {
      uint8_t configFinishedReceived: 1;
      uint8_t defaultConfig: 1;
      uint8_t weeklySchedule: 1;
      uint8_t altWeeklySchedule: 1;
      uint8_t ocrConfig: 1;
      uint8_t extendedDefaultConfig: 1;
    };
    uint8_t all = 0;
  };

 public:
  bool isSet(int configType) const;
  void clear(int configType);
  void clearAll();
  void setAll(uint8_t values);
  uint8_t getAll() const;
  void setConfigFinishedReceived();
  void clearConfigFinishedReceived();
  bool isConfigFinishedReceived() const;
  void set(int configType, bool value = true);
  bool operator!=(const ConfigTypesBitmap &other) const;
};
#pragma pack(pop)

class Condition;
class ActionHandler;
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

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
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void purgeConfig() override;

  void clearChannelConfigChangedFlag();

  void runAction(uint16_t event) const override;

  // returns true if function was changed (previous one was different)
  virtual bool setAndSaveFunction(_supla_int_t channelFunction);
  virtual bool loadFunctionFromConfig();
  virtual bool saveConfigChangeFlag() const;
  virtual bool loadConfigChangeFlag();
  bool isAnyUpdatePending() override;

  // methods to override for channels with runtime config support
  virtual ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                               bool local);
  virtual void fillChannelConfig(void *channelConfig, int *size, uint8_t index);

  void triggerSetChannelConfig(int configType = SUPLA_CONFIG_TYPE_DEFAULT);

 protected:
  bool iterateConfigExchange();
  /**
   * @brief Returns the next config type to be sent
   *
   * @return -1 if no more config types to be sent, otherwise the config type
   */
  int getNextConfigType() const;
  Supla::ChannelConfigState channelConfigState =
      Supla::ChannelConfigState::None;

  uint8_t setChannelConfigAttempts = 0;
  ConfigTypesBitmap usedConfigTypes;
  ConfigTypesBitmap receivedConfigTypes;
};

};  // namespace Supla

#endif  // SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
