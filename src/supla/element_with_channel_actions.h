// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
#define SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_

#include <stdint.h>
#include <supla-common/proto.h>
#include <supla/apply_config_result.h>
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
  LocalChangeSent = 6,
  LAST_STATE_MAX  // Sentinel: one past the last valid state.
};

static_assert(static_cast<uint8_t>(ChannelConfigState::LAST_STATE_MAX) <= 16,
              "ChannelConfigState does not fit in 4 bits");

#pragma pack(push, 1)
struct ConfigTypesBitmap {
 private:
  // This is a runtime-only bitmap. The local-change storage keeps the raw
  // value as uint32_t, so the bit assignments must remain compatible with
  // SUPLA_CONFIG_TYPE_* values used by legacy devices.
  uint8_t all = 0;

 public:
  bool isSet(int configType) const;
  void clear(int configType);
  void clearAll();
  void setAll(uint8_t values);
  uint8_t getAll() const;
  void set(int configType, bool value = true);
  bool operator!=(const ConfigTypesBitmap &other) const;
};
#pragma pack(pop)

static_assert(sizeof(ConfigTypesBitmap) == 1,
              "ConfigTypesBitmap must remain one byte");

class Condition;
class ActionHandler;
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol
class ElementWithChannelActions : public Element, public LocalAction {
 public:
  explicit ElementWithChannelActions(
      ElementMode mode = ElementMode::Registered);
  ~ElementWithChannelActions() override;

  // Override local action methods in order to delegate execution to Channel
  void addAction(uint16_t action,
      ActionHandler &client,  // NOLINT(runtime/references)
      uint16_t event,
      bool alwaysEnabled = false) override;
  void addAction(uint16_t action, ActionHandler *client, uint16_t event,
      bool alwaysEnabled = false) override;
  /**
   * Adds a conditional local action for a specific event.
   *
   * The condition source is this element and the condition client is the
   * provided action handler. The existing addAction(action, client, condition)
   * overload remains a shorthand for ON_CHANGE.
   */
  virtual void addAction(uint16_t action,
      ActionHandler &client,  // NOLINT(runtime/references)
      uint16_t event,
      Supla::Condition *condition,
      bool alwaysEnabled = false);
  /**
   * Pointer variant of addAction(action, client, event, condition).
   */
  virtual void addAction(uint16_t action, ActionHandler *client,
      uint16_t event,
      Supla::Condition *condition,
      bool alwaysEnabled = false);
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
  // Keep the deprecated Element overload visible for source compatibility.
  using Element::iterateConnected;
  void handleChannelConfigFinished() override;
  void handleChannelConfigFinished(int channelNumber) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *result,
                               bool altSchedule = false,
                               bool local = false) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void purgeConfig() override;


  void runAction(uint16_t event) const override;
  void runAction(
      uint16_t event,
      std::initializer_list<uint16_t> allowOnlyActions) const override;

  bool isAnyUpdatePending() const override;

  // methods to override for channels with runtime config support
  virtual ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                               bool local);
  virtual void fillChannelConfig(void *channelConfig, int *size, uint8_t index);

  void triggerSetChannelConfig(
      int configType = SUPLA_CONFIG_TYPE_DEFAULT,
      bool localChange = false);

 protected:
  // returns true if function was changed (previous one was different)
  virtual bool setAndSaveFunction(uint32_t channelFunction);
  virtual bool loadFunctionFromConfig();
  virtual bool shouldProcessChannelFunctionFromConfig() const;
  bool setAndSaveConfigChangeFlag(bool value);
  virtual bool saveConfigChangeFlag() const;
  virtual bool loadConfigChangeFlag();
  void clearChannelConfigChangedFlag();
  void markChannelConfigReceived(int configType);
  void markAllChannelConfigsReceived();
  bool isChannelConfigFinishedReceived() const;
  bool isLocalChannelConfigChangePending(int configType) const;
  bool iterateConfigExchange();
  /**
   * @brief Returns the next config type to be sent
   *
   * @return -1 if no more config types to be sent, otherwise the config type
   */
  int getNextConfigType() const;
  int getNextLocalConfigType() const;
  bool setLocalConfigChange(int configType, bool value = true);
  void clearLocalConfigChanges(int configType, int secondConfigType = -1);
  uint8_t getUsedLocalConfigTypes() const;
  // Keep exchange state in one byte. The retry counter must stay in 2 bits.
  ChannelConfigState channelConfigState : 4;
  uint8_t setChannelConfigAttempts : 2;
  uint8_t configFinishedReceived : 1;
  uint8_t reserved : 1;
  ConfigTypesBitmap locallyChangedConfigTypes;
  ConfigTypesBitmap usedConfigTypes;
  ConfigTypesBitmap receivedConfigTypes;

 private:
  uint8_t finishChannelConfig(
      TSD_ChannelConfig *result, Supla::ApplyConfigResult applyResult);
};

};  // namespace Supla

#endif  // SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
