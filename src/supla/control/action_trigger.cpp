// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_trigger.h"

#include <SuplaDevice.h>
#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla/local_action.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/control/button.h>
#include <supla/events.h>
#include <supla/storage/config_tags.h>
#include <supla/time.h>

#include "weekly_schedule_common.h"
#include "weekly_schedule_storage.h"

namespace {

// ActionTrigger state storage historically contains only a uint32_t with
// active actions. Keep that layout unchanged and use its two highest bits for
// the mutually exclusive weekly-schedule/locked state. If more state is
// needed, this format should be redesigned to use a wider dedicated field.
constexpr uint32_t kActionTriggerStateModeMask = 0xC0000000UL;
constexpr uint32_t kActionTriggerStateWeeklySchedule = 0x40000000UL;
constexpr uint32_t kActionTriggerStateLocked = 0x80000000UL;
constexpr uint32_t kActionTriggerLegacyAllActions = 0xFFFFFFFFUL;

}  // namespace

namespace Supla {
namespace Control {

class ActionTriggerWeeklySchedule : public NativeWeeklyScheduleController {
 public:
  explicit ActionTriggerWeeklySchedule(ActionTrigger *owner) : owner_(owner) {
  }

 protected:
  Supla::Element *getScheduleOwner() const override {
    return owner_;
  }

  const char *getDeviceLabel() const override {
    return "ActionTrigger";
  }

  const char *getScheduleStorageTag(bool alt) const override {
    (void)(alt);
    return Supla::ConfigTag::ActionTriggerWeeklyCfgTag;
  }

  bool validateSchedule(const TChannelConfig_WeeklySchedule *schedule,
                        bool alt) const override {
    (void)(alt);
    if (schedule == nullptr || owner_ == nullptr) {
      return false;
    }
    for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
      if (!owner_->isWeeklyScheduleProgramModeSupported(
              schedule->Program[i].Mode)) {
        SUPLA_LOG_WARNING(
            "ActionTrigger[%d]: invalid weekly schedule program %d",
            owner_->getChannelNumber(),
            i);
        return false;
      }
    }
    for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
      int programId = getProgramId(schedule, i);
      if (programId < 0 ||
          programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
        SUPLA_LOG_WARNING(
            "ActionTrigger[%d]: weekly schedule references invalid program %d",
            owner_->getChannelNumber(),
            programId);
        return false;
      }
    }
    return true;
  }

  void fillDefaultSchedule(TChannelConfig_WeeklySchedule *schedule,
                           bool alt) override {
    (void)(alt);
    if (owner_ != nullptr) {
      owner_->fillDefaultWeeklySchedule(schedule);
    }
  }

  void scheduleWeeklyScheduleStateSave() override {
    if (owner_ != nullptr) {
      owner_->scheduleStateSave();
    }
  }

  void syncWeeklyScheduleMode(uint8_t mode) override {
    if (owner_ == nullptr) {
      return;
    }
    owner_->channel.setWeeklyScheduleEnabled(isActive());
    owner_->applyButtonMode(isActive() ? mode : SUPLA_BUTTON_MODE_NOT_SET);
  }

 private:
  ActionTrigger *owner_ = nullptr;
};

}  // namespace Control
}  // namespace Supla

Supla::Control::ActionTrigger::ActionTrigger() {
  channel.setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_ACTIONTRIGGER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
}

Supla::Control::ActionTrigger::~ActionTrigger() {
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button *button) {
  if (attachedButton != nullptr && attachedButton != button) {
    attachedButton->setActionTriggerModeLocked(false, false, false);
  }
  attachedButton = button;
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED,
        localUnlockAllowed,
        keepConfigButtonTriggerAlwaysAvailable);
  }
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button &button) {
  attach(&button);
}

void Supla::Control::ActionTrigger::handleAction(int, int action) {
  if (!enabled) {
    return;
  }

  switch (action) {
    case Supla::LOCK:
      applyManualButtonMode(SUPLA_BUTTON_MODE_LOCKED);
      return;
    case Supla::UNLOCK:
      if (channel.getButtonMode() != SUPLA_BUTTON_MODE_LOCKED ||
          localUnlockAllowed) {
        applyManualButtonMode(SUPLA_BUTTON_MODE_NOT_SET);
      }
      return;
    case Supla::TOGGLE_LOCK:
      if (channel.getButtonMode() != SUPLA_BUTTON_MODE_LOCKED ||
          localUnlockAllowed) {
        applyManualButtonMode(
            channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED
                ? SUPLA_BUTTON_MODE_NOT_SET
                : SUPLA_BUTTON_MODE_LOCKED);
      }
      return;
  }

  if (channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED) {
    return;
  }
  uint32_t actionCap = getActionTriggerCap(action);

  if (actionCap & activeActionsFromServer ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer) {
    channel.pushAction(actionCap);
  }
}

Supla::Channel *Supla::Control::ActionTrigger::getChannel() {
  return &channel;
}

const Supla::Channel *Supla::Control::ActionTrigger::getChannel() const {
  return &channel;
}

void Supla::Control::ActionTrigger::activateAction(int action) {
  const uint32_t actionCap = getActionTriggerCap(action);
  if (actionCap != 0) {
    channel.activateAction(actionCap);
  }
}

int Supla::Control::ActionTrigger::getActionTriggerCap(int action) {
  switch (action) {
    case SEND_AT_TURN_ON: {
      return SUPLA_ACTION_CAP_TURN_ON;
    }
    case SEND_AT_TURN_OFF: {
      return SUPLA_ACTION_CAP_TURN_OFF;
    }
    case SEND_AT_TOGGLE_x1: {
      return SUPLA_ACTION_CAP_TOGGLE_x1;
    }
    case SEND_AT_TOGGLE_x2: {
      return SUPLA_ACTION_CAP_TOGGLE_x2;
    }
    case SEND_AT_TOGGLE_x3: {
      return SUPLA_ACTION_CAP_TOGGLE_x3;
    }
    case SEND_AT_TOGGLE_x4: {
      return SUPLA_ACTION_CAP_TOGGLE_x4;
    }
    case SEND_AT_TOGGLE_x5: {
      return SUPLA_ACTION_CAP_TOGGLE_x5;
    }
    case SEND_AT_HOLD: {
      return SUPLA_ACTION_CAP_HOLD;
    }
    case SEND_AT_SHORT_PRESS_x1: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x1;
    }
    case SEND_AT_SHORT_PRESS_x2: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x2;
    }
    case SEND_AT_SHORT_PRESS_x3: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x3;
    }
    case SEND_AT_SHORT_PRESS_x4: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x4;
    }
    case SEND_AT_SHORT_PRESS_x5: {
      return SUPLA_ACTION_CAP_SHORT_PRESS_x5;
    }
  }
  return 0;
}

int Supla::Control::ActionTrigger::actionTriggerCapToActionId(
    uint32_t actionCap) {
  switch (actionCap) {
    case SUPLA_ACTION_CAP_TURN_ON: {
      return SEND_AT_TURN_ON;
    }
    case SUPLA_ACTION_CAP_TURN_OFF: {
      return SEND_AT_TURN_OFF;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x1: {
      return SEND_AT_TOGGLE_x1;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x2: {
      return SEND_AT_TOGGLE_x2;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x3: {
      return SEND_AT_TOGGLE_x3;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x4: {
      return SEND_AT_TOGGLE_x4;
    }
    case SUPLA_ACTION_CAP_TOGGLE_x5: {
      return SEND_AT_TOGGLE_x5;
    }
    case SUPLA_ACTION_CAP_HOLD: {
      return SEND_AT_HOLD;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x1: {
      return SEND_AT_SHORT_PRESS_x1;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x2: {
      return SEND_AT_SHORT_PRESS_x2;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x3: {
      return SEND_AT_SHORT_PRESS_x3;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x4: {
      return SEND_AT_SHORT_PRESS_x4;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x5: {
      return SEND_AT_SHORT_PRESS_x5;
    }
  }
  return -1;
}

int Supla::Control::ActionTrigger::actionTriggerCapToButtonEvent(
    uint32_t actionCap) {
  switch (actionCap) {
    case SUPLA_ACTION_CAP_TURN_ON: {
      return Supla::ON_PRESS;
    }
    case SUPLA_ACTION_CAP_TURN_OFF: {
      return Supla::ON_RELEASE;
    }
    case SUPLA_ACTION_CAP_HOLD: {
      return Supla::ON_HOLD;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x1:
    case SUPLA_ACTION_CAP_TOGGLE_x1: {
      return Supla::ON_CLICK_1;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x2:
    case SUPLA_ACTION_CAP_TOGGLE_x2: {
      return Supla::ON_CLICK_2;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x3:
    case SUPLA_ACTION_CAP_TOGGLE_x3: {
      return Supla::ON_CLICK_3;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x4:
    case SUPLA_ACTION_CAP_TOGGLE_x4: {
      return Supla::ON_CLICK_4;
    }
    case SUPLA_ACTION_CAP_SHORT_PRESS_x5:
    case SUPLA_ACTION_CAP_TOGGLE_x5: {
      return Supla::ON_CLICK_5;
    }
  }
  return -1;
}

void Supla::Control::ActionTrigger::onRegistered(
    Supla::Protocol::SuplaSrpc *suplaSrpc) {
  Supla::ElementWithChannelActions::onRegistered(suplaSrpc);
  // cleanup actions to be send
  while (channel.popAction()) {
  }

  channel.enableValueUpdates();
  channel.setSendGetConfig();
}

void Supla::Control::ActionTrigger::parseActiveActionsFromServer() {
  if (actionHandlingType == ActionHandlingType_PublishAllDisableAll) {
    activeActionsFromServer = 0xFFFFFFFF;
  }

  uint32_t actionsToDisable =
    activeActionsFromServer & disablesLocalOperation;
  if (attachedButton) {
    bool makeSureThatOnClick1IsDisabled = false;
    bool makeSureThatOnChangePressReleaseIsDisabled = false;

    if (activeActionsFromServer ||
        actionHandlingType == ActionHandlingType_PublishAllDisableNone ||
        alwaysUseOnClick1) {
      // disable on_press, on_release, on_change local actions and enable
      // on_click_1
      if (localHandlerForDisabledAt && localHandlerForEnabledAt) {
        localHandlerForDisabledAt->disable();
        localHandlerForEnabledAt->enable();
        makeSureThatOnChangePressReleaseIsDisabled = true;
      }
    } else {
      // enable on_press, on_release, on_change local actions and
      // disable on_click_1
      if (localHandlerForDisabledAt && localHandlerForEnabledAt) {
        localHandlerForDisabledAt->enable();
        localHandlerForEnabledAt->disable();
        makeSureThatOnClick1IsDisabled = true;
      }
    }

    if (activeActionsFromServer & SUPLA_ACTION_CAP_HOLD) {
      attachedButton->disableRepeatOnHold(1000);
    } else {
      attachedButton->enableRepeatOnHold();
    }

    for (int i = 0; i < 32; i++) {
      uint32_t actionCap = (1ULL << i);
      int eventId = actionTriggerCapToButtonEvent(actionCap);
      int actionId = actionTriggerCapToActionId(actionCap);
      if (eventId == -1 || actionId == -1) {
        continue;
      }
      if (actionCap & activeActionsFromServer ||
          actionHandlingType == ActionHandlingType_PublishAllDisableNone) {
        attachedButton->enableAction(actionId, this, eventId);
      } else {
        if (eventId != Supla::ON_CLICK_1 || !alwaysUseOnClick1) {
          attachedButton->disableAction(actionId, this, eventId);
        }
      }

      // enable/disable other actions when AT from server is disabled/enabled
      if (actionsToDisable & actionCap) {
        attachedButton->disableOtherClients(this, eventId);
        if (eventId == Supla::ON_PRESS) {
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_PRESS);
        } else if (eventId == Supla::ON_RELEASE) {
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_RELEASE);
        } else if (eventId == Supla::ON_CLICK_1) {
          attachedButton->disableOtherClients(this,
                                              Supla::ON_CHANGE);
          attachedButton->disableOtherClients(this,
                                              Supla::CONDITIONAL_ON_CHANGE);
          if (attachedButton->isBistable()) {
            attachedButton->disableOtherClients(
                this, Supla::CONDITIONAL_ON_PRESS);
            attachedButton->disableOtherClients(
                this, Supla::CONDITIONAL_ON_RELEASE);
          }
        } else if (eventId == Supla::ON_HOLD) {
          attachedButton->disableOtherClients(this,
                                              Supla::ON_HOLD_RELEASE);
        }
      } else if (disablesLocalOperation & actionCap) {
        attachedButton->enableOtherClients(this, eventId);
        if (eventId == Supla::ON_PRESS) {
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_PRESS);
        } else if (eventId == Supla::ON_RELEASE) {
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_RELEASE);
        } else if (eventId == Supla::ON_CLICK_1) {
          attachedButton->enableOtherClients(this,
                                             Supla::ON_CHANGE);
          attachedButton->enableOtherClients(this,
                                             Supla::CONDITIONAL_ON_CHANGE);
          if (attachedButton->isBistable() &&
              !(activeActionsFromServer &
                (SUPLA_ACTION_CAP_TURN_ON | SUPLA_ACTION_CAP_TURN_OFF))) {
            attachedButton->enableOtherClients(
                this, Supla::CONDITIONAL_ON_PRESS);
            attachedButton->enableOtherClients(
                this, Supla::CONDITIONAL_ON_RELEASE);
          }
        } else if (eventId == Supla::ON_HOLD) {
          attachedButton->enableOtherClients(this,
                                             Supla::ON_HOLD_RELEASE);
        }
        if (makeSureThatOnClick1IsDisabled && eventId == Supla::ON_CLICK_1) {
          makeSureThatOnClick1IsDisabled = false;
          if (localHandlerForEnabledAt) {
            localHandlerForEnabledAt->disable();
          }
        }
        if (makeSureThatOnChangePressReleaseIsDisabled &&
            eventId == Supla::ON_CLICK_1) {
          makeSureThatOnChangePressReleaseIsDisabled = false;
          if (localHandlerForDisabledAt) {
            localHandlerForDisabledAt->disable();
          }
        }
      }
    }
    // Unlike ON_CHANGE, a directional pair cannot be cloned as one action on
    // ON_CLICK_1: the resolved input state selects press or release. Keep the
    // original handlers (and their individual AT masks), deferring only their
    // events. CFG x10 alone must not enable this policy.
    const bool directionalPair = attachedButton->isBistable() &&
        attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS, true) &&
        attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_RELEASE, true);
    attachedButton->setConditionalActionsOnClick1(
        directionalPair &&
        (activeActionsFromServer || alwaysUseOnClick1 ||
         actionHandlingType == ActionHandlingType_PublishAllDisableNone));
  }
}

Supla::ApplyConfigResult Supla::Control::ActionTrigger::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (result == nullptr) {
    return Supla::ApplyConfigResult::DataError;
  }
  if (result->ConfigType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    if (!weeklyScheduleAvailable) {
      return Supla::ApplyConfigResult::NotSupported;
    }
    ensureNativeWeeklyScheduleController();
    auto *configHandler = weeklyScheduleComponents.getConfigHandler();
    return configHandler == nullptr
               ? Supla::ApplyConfigResult::NotSupported
               : configHandler->applyChannelConfig(result, local);
  }
  if (result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return Supla::ApplyConfigResult::NotSupported;
  }
  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::Success;
  }
  if (result->ConfigSize != sizeof(TChannelConfig_ActionTrigger)) {
    return Supla::ApplyConfigResult::DataError;
  }

  TChannelConfig_ActionTrigger *config =
      reinterpret_cast<TChannelConfig_ActionTrigger *>(result->Config);
  Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());
  if (channelConfigReceived &&
      lastReceivedActiveActions == config->ActiveActions) {
    // Preserve an in-flight click/release when the server repeats its
    // configuration, e.g. after reconnecting. Local callers can still force
    // a rebuild when button bindings or the channel function change.
    return Supla::ApplyConfigResult::Success;
  }
  lastReceivedActiveActions = config->ActiveActions;
  channelConfigReceived = true;
  activeActionsFromServer = config->ActiveActions;
  SUPLA_LOG_DEBUG(
      "AT[%d] received config with active actions: 0x%X",
      channel.getChannelNumber(),
      activeActionsFromServer);
  rebuildForAttachedButton();
  // Schedule save in 2 s after state change
  scheduleStateSave(2000, 0);
  return Supla::ApplyConfigResult::Success;
}

bool Supla::Control::ActionTrigger::shouldProcessChannelFunctionFromConfig()
    const {
  return false;
}

void Supla::Control::ActionTrigger::setRelatedChannel(Element *element) {
  if (element && element->getChannel()) {
    setRelatedChannel(element->getChannel());
  }
}

void Supla::Control::ActionTrigger::setRelatedChannel(Element &element) {
  setRelatedChannel(&element);
}

void Supla::Control::ActionTrigger::setRelatedChannel(Channel *relatedChannel) {
  if (relatedChannel) {
    channel.setRelatedChannel(relatedChannel->getChannelNumber());
  }
}

void Supla::Control::ActionTrigger::setRelatedChannel(Channel &relatedChannel) {
  setRelatedChannel(&relatedChannel);
}

void Supla::Control::ActionTrigger::onInit() {
  rebuildForAttachedButton();
}

void Supla::Control::ActionTrigger::rebuildForAttachedButton() {
  if (!attachedButton) {
    parseActiveActionsFromServer();
    return;
  }

  if (attachedButton && localHandlerSwitchConfigured && localHandlerClient) {
    Supla::LocalAction::DeleteAction(attachedButton,
                                     localHandlerClient,
                                     Supla::ON_CLICK_1,
                                     localHandlerAction);
  }
  Supla::LocalAction::DeleteActionsHandledByExcept(
      this, {Supla::LOCK, Supla::UNLOCK, Supla::TOGGLE_LOCK});
  localHandlerForEnabledAt = nullptr;
  localHandlerForDisabledAt = nullptr;
  localHandlerClient = nullptr;
  localHandlerAction = 0;
  localHandlerSwitchConfigured = false;
  disablesLocalOperation = 0;
  channel.setActionTriggerCaps(0);

  // handle automatic switch from on_press, on_release, on_change
  // events to on_click_1 for local actions on relays, roller shutters, etc.
  if (attachedButton) {
    if (attachedButton->isBistable()) {
      bool isOnChangeUsed = attachedButton->isEventAlreadyUsed(
          Supla::ON_CHANGE, false);
      bool isConditionlOnChangeUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_CHANGE, false);

      if (isOnChangeUsed != isConditionlOnChangeUsed) {
        // for bistable button use on_change <-> on_click_1
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            isOnChangeUsed ? Supla::ON_CHANGE : Supla::CONDITIONAL_ON_CHANGE);
      }
    } else if (attachedButton->isMonostable()) {
      // for monostable button use on_press/on_release <-> on_click_1
      bool isOnPressUsed =
          attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, false);
      bool isOnReleaseUsed =
          attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, false);

      bool isConditionalOnPressUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_PRESS, false);
      bool isConditionalOnReleaseUsed = attachedButton->isEventAlreadyUsed(
          Supla::CONDITIONAL_ON_RELEASE, false);
      // check if only one of those bool values are set to true:
      if (isOnPressUsed && !isOnReleaseUsed && !isConditionalOnPressUsed &&
          !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt =
            attachedButton->getHandlerForFirstClient(Supla::ON_PRESS);
      } else if (isOnReleaseUsed && !isOnPressUsed &&
                 !isConditionalOnPressUsed && !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt =
            attachedButton->getHandlerForFirstClient(Supla::ON_RELEASE);
      } else if (isConditionalOnPressUsed && !isOnPressUsed &&
                 !isOnReleaseUsed && !isConditionalOnReleaseUsed) {
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            Supla::CONDITIONAL_ON_PRESS);
      } else if (isConditionalOnReleaseUsed && !isOnPressUsed &&
                 !isOnReleaseUsed && !isConditionalOnPressUsed) {
        localHandlerForDisabledAt = attachedButton->getHandlerForFirstClient(
            Supla::CONDITIONAL_ON_RELEASE);
      }
    } else if (attachedButton->isMotionSensor() ||
               attachedButton->isCentral()) {
      // Nothing to do here. For motion sensor and central we always use
      // reaction to on press and on release events. Even if AT is enabled and
      // used. So no localHandlerFor* is configured here.
    }

    if (localHandlerForDisabledAt) {
      localHandlerClient = localHandlerForDisabledAt->client;
      localHandlerAction = localHandlerForDisabledAt->action;
      attachedButton->addAction(localHandlerForDisabledAt->action,
                                localHandlerForDisabledAt->client,
                                Supla::ON_CLICK_1);
      localHandlerForEnabledAt = attachedButton->getHandlerForClient(
          localHandlerForDisabledAt->client, Supla::ON_CLICK_1);
      if (localHandlerForEnabledAt) {
        localHandlerForEnabledAt->disable();
        localHandlerSwitchConfigured = true;
      } else {
        localHandlerClient = nullptr;
        localHandlerAction = 0;
      }
    }
  }

  if (attachedButton) {
    // Configure default actions for bistable button
    if (attachedButton->isBistable()) {
      if (attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_PRESS,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_RELEASE,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
      }
      // Bistable roller shutters use conditional press/release for one
      // directional local action pair. Preserve the old bistable behavior:
      // TOGGLE_x1 must disable both local edges as one local operation.
      if (attachedButton->isEventAlreadyUsed(
              Supla::CONDITIONAL_ON_PRESS, true) &&
          attachedButton->isEventAlreadyUsed(
              Supla::CONDITIONAL_ON_RELEASE, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1, true) ||
          attachedButton->isEventAlreadyUsed(Supla::ON_CHANGE, true) ||
          attachedButton->isEventAlreadyUsed(Supla::CONDITIONAL_ON_CHANGE,
                                             true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x2;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x3;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x4;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x5;
      }

      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x1)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x1,
                                      Supla::ON_CLICK_1);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x2)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x2,
                                      Supla::ON_CLICK_2);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x3)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x3,
                                      Supla::ON_CLICK_3);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x4)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x4,
                                      Supla::ON_CLICK_4);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TOGGLE_x5)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TOGGLE_x5,
                                      Supla::ON_CLICK_5);
      }

    } else if (attachedButton->isMonostable()) {
      // Configure default actions for monostable button
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_HOLD, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_HOLD;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_1, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x1;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_2, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x2;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_3, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x3;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_4, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x4;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_CLICK_5, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_SHORT_PRESS_x5;
      }

      if (!(disabledCapabilities & SUPLA_ACTION_CAP_HOLD)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_HOLD, Supla::ON_HOLD);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x1)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x1,
                                      Supla::ON_CLICK_1);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x2)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x2,
                                      Supla::ON_CLICK_2);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x3)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x3,
                                      Supla::ON_CLICK_3);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x4)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x4,
                                      Supla::ON_CLICK_4);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_SHORT_PRESS_x5)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_SHORT_PRESS_x5,
                                      Supla::ON_CLICK_5);
      }

    } else if (attachedButton->isMotionSensor() ||
               attachedButton->isCentral()) {
      // Configure default actions for motion sensor and central buttons
      if (attachedButton->isEventAlreadyUsed(Supla::ON_PRESS, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
      }
      if (attachedButton->isEventAlreadyUsed(Supla::ON_RELEASE, true)) {
        disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_ON)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_ON, Supla::ON_PRESS);
      }
      if (!(disabledCapabilities & SUPLA_ACTION_CAP_TURN_OFF)) {
        addActionToButtonAndDisableIt(Supla::SEND_AT_TURN_OFF,
                                      Supla::ON_RELEASE);
      }
    }
  }

  channel.setDisablesLocalOperation(disablesLocalOperation);
  parseActiveActionsFromServer();
}

void Supla::Control::ActionTrigger::disableATCapability(uint32_t capToDisable) {
  disabledCapabilities |= capToDisable;
}

void Supla::Control::ActionTrigger::onSaveState() {
  if (!storageEnabled) {
    return;
  }

  ActionTriggerFlags flags = {};
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  flags.flags.weeklySchedule =
      weeklySchedule != nullptr && weeklySchedule->isActive();
  flags.flags.locked = !flags.flags.weeklySchedule &&
                       channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED;

  uint32_t state = activeActionsFromServer;
  // 0xFFFFFFFF is a legacy value meaning "all actions". Preserve it when
  // there is no mode flag to store; otherwise the two reserved bits encode
  // the current mode.
  if (state != kActionTriggerLegacyAllActions || flags.rawValue != 0) {
    state &= ~kActionTriggerStateModeMask;
    if (flags.flags.weeklySchedule) {
      state |= kActionTriggerStateWeeklySchedule;
    } else if (flags.flags.locked) {
      state |= kActionTriggerStateLocked;
    }
  }

  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&state),
                             sizeof(state));
}

void Supla::Control::ActionTrigger::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();

  ensureNativeWeeklyScheduleController();
  if (weeklyScheduleComponents.isAssigned()) {
    weeklyScheduleComponents.loadConfig();
  }

  int32_t value = 0;  // default value
  if (cfg != nullptr) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key,
        getChannel()->getChannelNumber(),
        Supla::ConfigTag::BtnActionTriggerCfgTagPrefix);
    cfg->getInt32(key, &value);

    int32_t localUnlockValue = 0;
    Supla::Config::generateKey(
        key,
        getChannel()->getChannelNumber(),
        Supla::ConfigTag::BtnActionTriggerLocalUnlockTagPrefix);
    if (cfg->getInt32(key, &localUnlockValue)) {
      localUnlockAllowed = localUnlockValue != 0;
    } else {
      localUnlockAllowed = false;
    }
    loadConfigChangeFlag();
  }
  setLocalUnlockAllowed(localUnlockAllowed);

  const auto previousHandlingType = actionHandlingType;
  switch (value) {
    case 0:
    default: {
      // rely on configuration from Supla server
      // In MQTT mode only actions enabled on Supla server are published on
      // MQTT.
      actionHandlingType = ActionHandlingType_RelayOnSuplaServer;
      break;
    }
    case 1: {
      // All events will be published to Supla server and/or MQTT.
      // Local actions are disabled only if it is done on Supla server
      actionHandlingType = ActionHandlingType_PublishAllDisableNone;
      break;
    }
    case 2: {
      // disable local input regardless of Supla server config
      // All events are published
      actionHandlingType = ActionHandlingType_PublishAllDisableAll;
      break;
    }
  }
  if (previousHandlingType != actionHandlingType) {
    channelConfigReceived = false;
  }
}

void Supla::Control::ActionTrigger::onLoadState() {
  if (!storageEnabled) {
    return;
  }

  channelConfigReceived = false;

  uint32_t state = activeActionsFromServer;
  const bool stateLoaded = Supla::Storage::ReadState(
      reinterpret_cast<unsigned char *>(&state), sizeof(state));

  ActionTriggerFlags flags = {};
  if (!stateLoaded) {
    return;
  }

  flags.rawValue = 0;
  if (state == kActionTriggerLegacyAllActions) {
    // Keep compatibility with the old all-actions sentinel, which predates
    // the two mode bits.
    activeActionsFromServer = state;
  } else {
    switch (state & kActionTriggerStateModeMask) {
      case kActionTriggerStateWeeklySchedule:
        flags.flags.weeklySchedule = 1;
        break;
      case kActionTriggerStateLocked:
        flags.flags.locked = 1;
        break;
      default:
        break;
    }
    activeActionsFromServer = state & ~kActionTriggerStateModeMask;
  }

  if (activeActionsFromServer) {
    SUPLA_LOG_INFO(
        "AT[%d]: restored activeActionsFromServer: 0x%X",
        channel.getChannelNumber(),
        activeActionsFromServer);
  }

  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr && isWeeklyScheduleSupported()) {
    weeklySchedule->restoreWeeklyScheduleMode(flags.flags.weeklySchedule);
    if (weeklySchedule->isExternallyManaged()) {
      channel.setWeeklyScheduleEnabled(weeklySchedule->isActive());
      applyButtonMode(SUPLA_BUTTON_MODE_NOT_SET);
    }
  }
  if (!flags.flags.weeklySchedule) {
    applyButtonMode(flags.flags.locked ? SUPLA_BUTTON_MODE_LOCKED
                                       : SUPLA_BUTTON_MODE_NOT_SET);
  }
}

void Supla::Control::ActionTrigger::enableStateStorage() {
  storageEnabled = true;
}

void Supla::Control::ActionTrigger::addActionToButtonAndDisableIt(int action,
                                                                  int event) {
  attachedButton->addAction(action, this, event);
  attachedButton->disableAction(action, this, event);
}

bool Supla::Control::ActionTrigger::isAnyActionEnabledOnServer() const {
  return activeActionsFromServer != 0;
}

void Supla::Control::ActionTrigger::setAlwaysUseOnClick1() {
  alwaysUseOnClick1 = true;
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setLocalUnlockAllowed(bool allowed) {
  localUnlockAllowed = allowed;
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED,
        localUnlockAllowed,
        keepConfigButtonTriggerAlwaysAvailable);
  }
  return *this;
}

bool Supla::Control::ActionTrigger::isLocalUnlockAllowed() const {
  return localUnlockAllowed;
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setKeepConfigButtonTriggerAlwaysAvailable(
    bool keep) {
  keepConfigButtonTriggerAlwaysAvailable = keep;
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED,
        localUnlockAllowed,
        keepConfigButtonTriggerAlwaysAvailable);
  }
  return *this;
}

bool Supla::Control::ActionTrigger::
    keepsConfigButtonTriggerAlwaysAvailable() const {
  return keepConfigButtonTriggerAlwaysAvailable;
}

void Supla::Control::ActionTrigger::enable() {
  enabled = true;
}

void Supla::Control::ActionTrigger::disable() {
  enabled = false;
}

bool Supla::Control::ActionTrigger::setWeeklyScheduleController(
    WeeklyScheduleController *controller,
    WeeklyScheduleConfigHandler *configHandler,
    WeeklyScheduleProgramSource *programSource) {
  if (!weeklyScheduleComponents.set(
          controller, configHandler, programSource)) {
    return false;
  }
  updateWeeklyScheduleCapabilities();
  return true;
}

bool Supla::Control::ActionTrigger::ensureNativeWeeklyScheduleController() {
  if (!weeklyScheduleAvailable || weeklyScheduleComponents.isAssigned()) {
    return false;
  }
  auto *weeklySchedule = new ActionTriggerWeeklySchedule(this);
  if (!setWeeklyScheduleController(
          weeklySchedule, weeklySchedule, weeklySchedule)) {
    delete weeklySchedule;
    return false;
  }
  return true;
}

void Supla::Control::ActionTrigger::updateWeeklyScheduleCapabilities() {
  channel.setFlag(SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);
  auto *controller = weeklyScheduleComponents.getController();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  bool configurable =
      weeklyScheduleAvailable && configHandler != nullptr &&
      configHandler->supportsConfigType(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  if (configurable) {
    channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  } else {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    usedConfigTypes.clear(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  }
  if (!weeklyScheduleAvailable && controller != nullptr) {
    controller->switchToManualMode();
  }
}

bool Supla::Control::ActionTrigger::isWeeklyScheduleSupported() const {
  return weeklyScheduleAvailable;
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setWeeklyScheduleAvailable(bool available) {
  weeklyScheduleAvailable = available;
  if (available) {
    ensureNativeWeeklyScheduleController();
  }
  updateWeeklyScheduleCapabilities();
  return *this;
}

void Supla::Control::ActionTrigger::fillDefaultWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule) {
  (void)(schedule);
}

bool Supla::Control::ActionTrigger::isWeeklyScheduleProgramModeSupported(
    uint8_t mode) const {
  return mode == SUPLA_BUTTON_MODE_NOT_SET ||
         mode == SUPLA_BUTTON_MODE_LOCKED;
}

void Supla::Control::ActionTrigger::iterateAlways() {
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr && isWeeklyScheduleSupported()) {
    weeklySchedule->processWeeklySchedule();
  }
}

int32_t Supla::Control::ActionTrigger::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  if (newValue == nullptr) {
    return -1;
  }
  auto *properties =
      reinterpret_cast<TActionTriggerProperties *>(newValue->value);
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  switch (properties->ButtonMode) {
    case SUPLA_BUTTON_MODE_LOCKED:
    case SUPLA_BUTTON_MODE_NOT_SET: {
      applyManualButtonMode(properties->ButtonMode);
      return 1;
    }
    case SUPLA_BUTTON_MODE_CMD_SWITCH_TO_MANUAL: {
      applyManualButtonMode(SUPLA_BUTTON_MODE_NOT_SET);
      return 1;
    }
    case SUPLA_BUTTON_MODE_CMD_WEEKLY_SCHEDULE: {
      if (weeklySchedule != nullptr && isWeeklyScheduleSupported() &&
          weeklySchedule->switchToWeeklySchedule()) {
        if (weeklySchedule->isExternallyManaged()) {
          channel.setWeeklyScheduleEnabled(weeklySchedule->isActive());
          applyButtonMode(SUPLA_BUTTON_MODE_NOT_SET);
        }
        scheduleStateSave();
        return 1;
      }
      return 0;
    }
    default: {
      return -1;
    }
  }
}

void Supla::Control::ActionTrigger::fillChannelConfig(
    void *channelConfig, int *size, uint8_t configType) {
  if (size == nullptr) {
    return;
  }
  *size = 0;
  if (channelConfig == nullptr ||
      configType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    return;
  }
  ensureNativeWeeklyScheduleController();
  updateWeeklyScheduleCapabilities();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  if (configHandler != nullptr && isWeeklyScheduleSupported()) {
    configHandler->fillChannelConfig(channelConfig, size, configType);
  }
}

void Supla::Control::ActionTrigger::purgeConfig() {
  ElementWithChannelActions::purgeConfig();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  if (configHandler != nullptr) {
    configHandler->purgeConfig();
  }
}

void Supla::Control::ActionTrigger::applyButtonMode(uint8_t mode) {
  channel.setButtonMode(mode);
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        mode == SUPLA_BUTTON_MODE_LOCKED,
        localUnlockAllowed,
        keepConfigButtonTriggerAlwaysAvailable);
  }
}

void Supla::Control::ActionTrigger::applyManualButtonMode(uint8_t mode) {
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr) {
    weeklySchedule->switchToManualMode();
  }
  channel.setWeeklyScheduleEnabled(false);
  applyButtonMode(mode);
  scheduleStateSave();
}

void Supla::Control::ActionTrigger::scheduleStateSave(uint32_t delayMsMax,
                                                       uint32_t delayMsMin) {
  if (storageEnabled) {
    Supla::Storage::ScheduleSave(delayMsMax, delayMsMin);
  }
}
