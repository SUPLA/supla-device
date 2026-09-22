// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_
#define SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_

#include <stdint.h>

#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/at_channel.h>
#include <supla/control/weekly_schedule_component.h>
#include <supla/element_with_channel_actions.h>

namespace Supla {

namespace Protocol {
class SuplaSrpc;
}

enum ActionHandlingType : uint8_t {
  ActionHandlingType_RelayOnSuplaServer = 0,
  ActionHandlingType_PublishAllDisableNone = 1,
  ActionHandlingType_PublishAllDisableAll = 2
};

namespace Control {

class Button;
class ActionTriggerWeeklySchedule;

class ActionTrigger : public ElementWithChannelActions, public ActionHandler {
 public:
  friend class ActionTriggerWeeklySchedule;

  union ActionTriggerFlags {
    struct {
      uint8_t weeklySchedule : 1;
      uint8_t locked : 1;
      uint8_t reserved : 6;
    } flags;
    uint8_t rawValue = 0;
  };

  static_assert(sizeof(ActionTriggerFlags) == sizeof(uint8_t),
                "Flags size must be 1 byte");

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
  const Supla::Channel *getChannel() const override;
  void onInit() override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr) override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onLoadState() override;
  void onSaveState() override;
  void iterateAlways() override;
  int32_t handleNewValueFromServer(
      TSD_SuplaChannelNewValue *newValue) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  void purgeConfig() override;

  void rebuildForAttachedButton();
  void disableATCapability(uint32_t capToDisable);
  void enableStateStorage();

  static int actionTriggerCapToButtonEvent(uint32_t actionCap);
  static int actionTriggerCapToActionId(uint32_t actionCap);
  static int getActionTriggerCap(int action);

  bool isAnyActionEnabledOnServer() const;

  void setAlwaysUseOnClick1();

  // Controls whether a locked AT button may execute local UNLOCK or
  // TOGGLE_LOCK actions. The default keeps the historic full lock behavior.
  ActionTrigger &setLocalUnlockAllowed(bool allowed = true);
  bool isLocalUnlockAllowed() const;

  // Explicit opt-in for a shared configuration button. This does not make
  // every alwaysEnabled action bypass an AT lock; only the two built-in
  // configuration actions are retained by Button when this is enabled.
  ActionTrigger &setKeepConfigButtonTriggerAlwaysAvailable(
      bool keep = true);
  bool keepsConfigButtonTriggerAlwaysAvailable() const;

  void enable();
  void disable();

  bool isWeeklyScheduleSupported() const;
  ActionTrigger &setWeeklyScheduleAvailable(bool available = true);
  bool setWeeklyScheduleController(
      WeeklyScheduleController *controller,
      WeeklyScheduleConfigHandler *configHandler = nullptr,
      WeeklyScheduleProgramSource *programSource = nullptr);

 protected:
  ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                       bool local) override;
  bool shouldProcessChannelFunctionFromConfig() const override;
  virtual void fillDefaultWeeklySchedule(
      TChannelConfig_WeeklySchedule *schedule);
  virtual bool isWeeklyScheduleProgramModeSupported(uint8_t mode) const;
  bool ensureNativeWeeklyScheduleController();
  void updateWeeklyScheduleCapabilities();
  void scheduleStateSave(uint32_t delayMsMax = 5000,
                         uint32_t delayMsMin = 2000);
  void applyButtonMode(uint8_t mode);
  void applyManualButtonMode(uint8_t mode);
  void addActionToButtonAndDisableIt(int event, int action);
  void parseActiveActionsFromServer();

  Supla::Control::Button *attachedButton = nullptr;
  Supla::ActionHandlerClient *localHandlerForEnabledAt = nullptr;
  Supla::ActionHandlerClient *localHandlerForDisabledAt = nullptr;
  Supla::ActionHandler *localHandlerClient = nullptr;
  uint32_t activeActionsFromServer = 0;
  // Comparison cache only: PublishAllDisableAll overwrites the effective mask
  // above. Keep state storage and its existing format unchanged.
  uint32_t lastReceivedActiveActions = 0;
  uint32_t disablesLocalOperation = 0;
  uint32_t disabledCapabilities = 0;

  Supla::AtChannel channel;

  uint16_t localHandlerAction = 0;
  ActionHandlingType actionHandlingType = ActionHandlingType_RelayOnSuplaServer;
  // Runtime-only flags; state storage uses a separate representation.
  struct RuntimeFlags {
    RuntimeFlags()
        : storageEnabled(false),
          alwaysUseOnClick1(false),
          enabled(true),
          localHandlerSwitchConfigured(false),
          channelConfigReceived(false),
          weeklyScheduleAvailable(false),
          localUnlockAllowed(false),
          keepConfigButtonTriggerAlwaysAvailable(false) {}

    uint8_t storageEnabled : 1;
    uint8_t alwaysUseOnClick1 : 1;
    uint8_t enabled : 1;
    uint8_t localHandlerSwitchConfigured : 1;
    uint8_t channelConfigReceived : 1;
    // Native weekly schedule remains opt-in.
    uint8_t weeklyScheduleAvailable : 1;
    uint8_t localUnlockAllowed : 1;
    uint8_t keepConfigButtonTriggerAlwaysAvailable : 1;
  };

  static_assert(sizeof(RuntimeFlags) == sizeof(uint8_t),
                "ActionTrigger runtime flags must fit in uint8_t");

  RuntimeFlags runtimeFlags;
  WeeklyScheduleComponents weeklyScheduleComponents;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_ACTION_TRIGGER_H_
