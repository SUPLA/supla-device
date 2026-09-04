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
  LocalChangeSent = 6
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
namespace Control {
class WeeklyScheduleController;
class WeeklyScheduleConfigHandler;
}  // namespace Control

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
  void handleChannelConfigFinished() override;
  void handleChannelConfigFinished(int channelNumber) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *result, bool local) override;
  uint8_t handleWeeklySchedule(TSD_ChannelConfig *result,
                               bool altSchedule,
                               bool local) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void purgeConfig() override;


  void runAction(uint16_t event) const override;

  bool isAnyUpdatePending() const override;

  // methods to override for channels with runtime config support
  virtual ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                               bool local);
  virtual void fillChannelConfig(void *channelConfig, int *size, uint8_t index);

  void triggerSetChannelConfig(
      int configType = SUPLA_CONFIG_TYPE_DEFAULT,
      bool localChange = false);
  // Replaces the owned runtime controller before configuration loading begins.
  // Ownership of the controller is transferred only when true is returned.
  bool setWeeklyScheduleController(
      Supla::Control::WeeklyScheduleController *controller);

 protected:
  // returns true if function was changed (previous one was different)
  virtual bool setAndSaveFunction(uint32_t channelFunction);
  virtual bool loadFunctionFromConfig();
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
  void loadWeeklyScheduleConfig();
  bool isWeeklyScheduleControllerAssigned() const;
  bool isWeeklyScheduleLifecycleStarted() const;
  Supla::Control::WeeklyScheduleController *getWeeklyScheduleController() const;
  Supla::Control::WeeklyScheduleConfigHandler *
  getWeeklyScheduleConfigHandler() const;
  virtual void onWeeklyScheduleControllerChanged(
      Supla::Control::WeeklyScheduleController *controller);
  uint8_t handleWeeklyScheduleWithConfigHandler(
      TSD_ChannelConfig *result,
      bool altSchedule,
      bool local,
      Supla::Control::WeeklyScheduleConfigHandler *configHandler);
  Supla::ChannelConfigState channelConfigState =
      Supla::ChannelConfigState::None;

  uint8_t setChannelConfigAttempts = 0;
  // Bit number maps directly to SUPLA_CONFIG_TYPE_*. NVS stores the bitmap as
  // uint32_t, but currently only four types are locally changeable (DEFAULT,
  // WEEKLY_SCHEDULE, ALT_WEEKLY_SCHEDULE and EXTENDED), and all their type IDs
  // fit in 0..7. Widen this field and the related helpers to uint16_t or
  // uint32_t before adding a locally changed config type >= 8.
  uint8_t locallyChangedConfigTypes = 0;
  ConfigTypesBitmap usedConfigTypes;
  ConfigTypesBitmap receivedConfigTypes;

 private:
  enum class WeeklyScheduleLifecycleState : uint8_t {
    Unassigned,
    Assigned,
    Started,
  };

  uint8_t finishChannelConfig(
      TSD_ChannelConfig *result, Supla::ApplyConfigResult applyResult);
  WeeklyScheduleLifecycleState weeklyScheduleLifecycleState_ =
      WeeklyScheduleLifecycleState::Unassigned;
  Supla::Control::WeeklyScheduleController *weeklyScheduleController_ = nullptr;
};

};  // namespace Supla

#endif  // SRC_SUPLA_ELEMENT_WITH_CHANNEL_ACTIONS_H_
