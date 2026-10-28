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
#ifndef ARDUINO_ARCH_AVR
#include <new>
#endif

#include "weekly_schedule_common.h"
#include "weekly_schedule_storage.h"

namespace {

// Protocol v29 limits all temporary unlock durations to 18 hours.
constexpr uint32_t kMaximumTemporaryUnlockDurationS = 64800;

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
      const auto &program = schedule->Program[i];
      if (!owner_->isWeeklyScheduleProgramModeSupported(program.Mode) ||
          (program.Mode == SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED &&
           (program.Value2 != 0 || program.ButtonTemporaryUnlockDurationS >
               kMaximumTemporaryUnlockDurationS))) {
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

  bool applyResolvedWeeklyScheduleProgram(
      const TWeeklyScheduleProgram &program, int programId,
      bool programChanged) override {
    (void)programChanged;
    if (programId > 0 &&
        program.Mode == SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED &&
        !owner_->prepareTemporaryUnlockState()) {
      // Fail closed if the runtime allocation fails.
      owner_->applyButtonMode(
          SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED);
      return false;
    }
    if (owner_->temporaryUnlock) {
      owner_->temporaryUnlock->weeklyDurationS = programId > 0 &&
          program.Mode == SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED
              ? program.ButtonTemporaryUnlockDurationS : 0;
    }
    syncWeeklyScheduleMode(programId > 0 ? program.Mode : 0);
    return true;
  }

  void syncWeeklyScheduleMode(uint8_t mode) override {
    if (owner_ == nullptr ||
        (owner_->temporaryUnlock &&
         owner_->temporaryUnlock->manualModeTransition)) {
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

namespace {
// Recognize an actual directional pair, not arbitrary conditional handlers.
bool isDirectionalHandler(const Supla::ActionHandlerClient *handler) {
  int press = -1;
  int release = -1;
  switch (handler->action) {
    case Supla::MOVE_UP:
    case Supla::UP_STOP:
      press = Supla::MOVE_UP;
      release = Supla::UP_STOP;
      break;
    case Supla::MOVE_DOWN:
    case Supla::DOWN_STOP:
      press = Supla::MOVE_DOWN;
      release = Supla::DOWN_STOP;
      break;
    case Supla::INTERNAL_BUTTON_MOVE_UP:
    case Supla::INTERNAL_BUTTON_UP_STOP:
      press = Supla::INTERNAL_BUTTON_MOVE_UP;
      release = Supla::INTERNAL_BUTTON_UP_STOP;
      break;
    case Supla::INTERNAL_BUTTON_MOVE_DOWN:
    case Supla::INTERNAL_BUTTON_DOWN_STOP:
      press = Supla::INTERNAL_BUTTON_MOVE_DOWN;
      release = Supla::INTERNAL_BUTTON_DOWN_STOP;
      break;
    default:
      return false;
  }
  const bool isPress = handler->action == press &&
      handler->onEvent == Supla::CONDITIONAL_ON_PRESS;
  const bool isRelease = handler->action == release &&
      handler->onEvent == Supla::CONDITIONAL_ON_RELEASE;
  if (!handler->client || (!isPress && !isRelease)) {
    return false;
  }
  for (auto other = Supla::ActionHandlerClient::begin; other;
       other = other->next) {
    if (other->trigger == handler->trigger &&
        other->client == handler->client &&
        other->action == (isPress ? release : press) &&
        other->onEvent == (isPress ? Supla::CONDITIONAL_ON_RELEASE
                                  : Supla::CONDITIONAL_ON_PRESS)) {
      return true;
    }
  }
  return false;
}
}  // namespace

Supla::Control::ActionTrigger::ActionTrigger() {
  channel.setType(SUPLA_CHANNELTYPE_ACTIONTRIGGER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_BUTTON_TEMPORARY_UNLOCK_SUPPORTED);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_ACTIONTRIGGER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_BUTTON_MODE_SUPPORTED);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

Supla::Control::ActionTrigger::~ActionTrigger() {
  delete temporaryUnlock;
  temporaryUnlock = nullptr;
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button *button) {
  if (attachedButton != nullptr && attachedButton != button) {
    attachedButton->setTemporaryUnlockDeadline(0, 0);
    attachedButton->setActionTriggerModeLocked(false, false, false);
  }
  attachedButton = button;
  if (attachedButton != nullptr) {
    attachedButton->setTemporaryUnlockDeadline(
        temporaryUnlock ? temporaryUnlock->startMs : 0,
        isTemporaryUnlockActive() ? temporaryUnlock->durationMs : 0);
    attachedButton->setActionTriggerModeLocked(
        isEffectivelyLocked(),
        runtimeFlags.localUnlockAllowed,
        runtimeFlags.keepConfigButtonTriggerAlwaysAvailable);
  }
}

void Supla::Control::ActionTrigger::attach(Supla::Control::Button &button) {
  attach(&button);
}

void Supla::Control::ActionTrigger::handleAction(int, int action) {
  if (!runtimeFlags.enabled) {
    return;
  }

  switch (action) {
    case Supla::LOCK:
      applyManualButtonMode(SUPLA_BUTTON_MODE_LOCKED);
      return;
    case Supla::UNLOCK:
      if (!isEffectivelyLocked() ||
          runtimeFlags.localUnlockAllowed) {
        applyManualButtonMode(SUPLA_BUTTON_MODE_NOT_SET);
      }
      return;
    case Supla::TOGGLE_LOCK:
      if (!isEffectivelyLocked() ||
          runtimeFlags.localUnlockAllowed) {
        applyManualButtonMode(
            isEffectivelyLocked()
                ? SUPLA_BUTTON_MODE_NOT_SET
                : SUPLA_BUTTON_MODE_LOCKED);
      }
      return;
  }

  updateTemporaryUnlock();
  if (isEffectivelyLocked()) {
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
    case SEND_AT_ROTATE_RIGHT:
      return SUPLA_ACTION_CAP_ROTATE_RIGHT;
    case SEND_AT_ROTATE_LEFT:
      return SUPLA_ACTION_CAP_ROTATE_LEFT;
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
    case SUPLA_ACTION_CAP_ROTATE_RIGHT:
      return SEND_AT_ROTATE_RIGHT;
    case SUPLA_ACTION_CAP_ROTATE_LEFT:
      return SEND_AT_ROTATE_LEFT;
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
        runtimeFlags.alwaysUseOnClick1) {
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
      if ((actionCap & activeActionsFromServer) ||
          actionHandlingType == ActionHandlingType_PublishAllDisableNone) {
        attachedButton->enableAction(actionId, this, eventId);
      } else {
        if (eventId != Supla::ON_CLICK_1 || !runtimeFlags.alwaysUseOnClick1) {
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
    // TURN_ON/OFF publish raw edges. Only TOGGLE_x1 overrides the paired
    // local motor operation, independently of other handlers on these events.
    if (attachedButton->isBistable()) {
      for (auto handler = Supla::ActionHandlerClient::begin; handler;
           handler = handler->next) {
        if (handler->trigger == attachedButton &&
            isDirectionalHandler(handler)) {
          if (activeActionsFromServer & SUPLA_ACTION_CAP_TOGGLE_x1) {
            handler->disable();
          } else {
            handler->enable();
          }
        }
      }
    }
  }
  synchronizeDirectionalButtonModes();
}

bool Supla::Control::ActionTrigger::hasDirectionalPair() const {
  if (!attachedButton || !attachedButton->isBistable()) {
    return false;
  }
  for (auto handler = Supla::ActionHandlerClient::begin; handler;
       handler = handler->next) {
    if (handler->trigger == attachedButton && isDirectionalHandler(handler)) {
      return true;
    }
  }
  return false;
}

bool Supla::Control::ActionTrigger::requiresClickMode() const {
  return activeActionsFromServer || runtimeFlags.alwaysUseOnClick1 ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer;
}

void Supla::Control::ActionTrigger::synchronizeDirectionalButtonModes() {
  // Configuration-time scan only: no peer pointers, extra RAM or timer work.
  for (auto element = Supla::Element::begin(); element;
       element = element->next()) {
    auto at = element->getActionTrigger();
    if (!at || !at->attachedButton) {
      continue;
    }
    bool useClicks = false;
    if (at->hasDirectionalPair()) {
      useClicks = at->requiresClickMode();
      const auto related = at->channel.getRelatedChannelNumber();
      if (related && !useClicks) {
        for (auto peerElement = Supla::Element::begin(); peerElement;
             peerElement = peerElement->next()) {
          auto peer = peerElement->getActionTrigger();
          if (peer && peer->channel.getRelatedChannelNumber() == related &&
              peer->hasDirectionalPair() && peer->requiresClickMode()) {
            useClicks = true;
            break;
          }
        }
      }
    }
    at->attachedButton->setConditionalActionsOnClick1(useClicks);
  }
}

Supla::ApplyConfigResult Supla::Control::ActionTrigger::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (result == nullptr) {
    return Supla::ApplyConfigResult::DataError;
  }
  if (result->ConfigType == SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE) {
    if (!runtimeFlags.weeklyScheduleAvailable) {
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
  if (result->ConfigSize != 4 &&
      result->ConfigSize != sizeof(TChannelConfig_ActionTrigger)) {
    return Supla::ApplyConfigResult::DataError;
  }
  Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());
  uint32_t activeActions = 0;
  memcpy(&activeActions, result->Config, sizeof(activeActions));
  auto candidate = currentTemporaryUnlockConfig();
  if (result->ConfigSize == sizeof(TChannelConfig_ActionTrigger)) {
    TChannelConfig_ActionTrigger wire = {};
    memcpy(&wire, result->Config, sizeof(wire));
    if (wire.TemporaryUnlockDurationSec > kMaximumTemporaryUnlockDurationS ||
        wire.TemporaryUnlockSensorEdge > SUPLA_AT_SENSOR_EDGE_BOTH) {
      return Supla::ApplyConfigResult::DataError;
    }
    candidate.duration = wire.TemporaryUnlockDurationSec;
    candidate.edge = wire.TemporaryUnlockSensorEdge;
    if (wire.TemporaryUnlockSensorIsSet > 1 ||
        (wire.TemporaryUnlockSensorIsSet &&
         wire.TemporaryUnlockSensorChannelNo == 255)) {
      return Supla::ApplyConfigResult::DataError;
    }
    candidate.sensor = wire.TemporaryUnlockSensorIsSet
        ? wire.TemporaryUnlockSensorChannelNo : 255;
  }
  const uint32_t effectiveActions =
      actionHandlingType == ActionHandlingType_PublishAllDisableAll
          ? kActionTriggerLegacyAllActions : activeActions;
  if (channel.getButtonMode() ==
      SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED) {
    candidate.escapedActions =
        (effectiveActions & ~kActionTriggerStateModeMask) == 0x3FFFFFFFUL
            ? effectiveActions : 0;
  }
  const auto currentConfig = currentTemporaryUnlockConfig();
  const bool configChanged =
      memcmp(&candidate, &currentConfig, sizeof(candidate)) != 0;
  const bool activeActionsChanged = !runtimeFlags.channelConfigReceived ||
      lastReceivedActiveActions != activeActions;
  const bool localChange = local && (configChanged || activeActionsChanged);
  const bool storeActiveActions = activeActionsChanged &&
      (temporaryUnlock ||
       actionHandlingType == ActionHandlingType_PublishAllDisableAll);
  if (configChanged && !ensureTemporaryUnlockState()) {
    return Supla::ApplyConfigResult::DataError;
  }
  if ((configChanged || localChange || storeActiveActions ||
       (temporaryUnlock && temporaryUnlock->persistenceUncertain)) &&
      !persistTemporaryUnlockConfig(candidate, localChange, &activeActions)) {
    return Supla::ApplyConfigResult::DataError;
  }
  if (configChanged) {
    temporaryUnlock->config = candidate;
    temporaryUnlock->baselineKnown = false;
  }
  if (localChange) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
  }
  if (!activeActionsChanged) {
    return Supla::ApplyConfigResult::Success;
  }
  lastReceivedActiveActions = activeActions;
  runtimeFlags.channelConfigReceived = true;
  activeActionsFromServer = activeActions;
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
    synchronizeDirectionalButtonModes();
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

  // Explicit rebuilds and changed masks discard this input's old sequence.
  // Peer synchronization alone must preserve its pending STOP/click deadline.
  if (attachedButton->runtimeFlags.conditionalActionsOnClick1) {
    attachedButton->clickCounter = 0;
    attachedButton->holdSend = 0;
  }

  if (attachedButton && runtimeFlags.localHandlerSwitchConfigured &&
      localHandlerClient) {
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
  runtimeFlags.localHandlerSwitchConfigured = false;
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
        runtimeFlags.localHandlerSwitchConfigured = true;
      } else {
        localHandlerClient = nullptr;
        localHandlerAction = 0;
      }
    }
  }

  if (attachedButton) {
    // Configure default actions for bistable button
    if (attachedButton->isBistable()) {
      for (auto handler = Supla::ActionHandlerClient::begin; handler;
           handler = handler->next) {
        if (handler->trigger != attachedButton || handler->isAlwaysEnabled()) {
          continue;
        }
        if (isDirectionalHandler(handler)) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TOGGLE_x1;
        } else if (handler->onEvent == Supla::ON_PRESS ||
                   handler->onEvent == Supla::CONDITIONAL_ON_PRESS) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_ON;
        } else if (handler->onEvent == Supla::ON_RELEASE ||
                   handler->onEvent == Supla::CONDITIONAL_ON_RELEASE) {
          disablesLocalOperation |= SUPLA_ACTION_CAP_TURN_OFF;
        }
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
  if (!runtimeFlags.storageEnabled) {
    return;
  }
  Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());

  ActionTriggerFlags flags = {};
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  flags.flags.weeklySchedule =
      weeklySchedule != nullptr && weeklySchedule->isActive();
  flags.flags.locked = !flags.flags.weeklySchedule &&
                       channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED;
  const bool temporaryMode = !flags.flags.weeklySchedule &&
      channel.getButtonMode() ==
          SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED;

  uint32_t state = activeActionsFromServer;
  // 0xFFFFFFFF is a legacy value meaning "all actions". Preserve it when
  // there is no mode flag to store; otherwise the two reserved bits encode
  // the current mode.
  if (state != kActionTriggerLegacyAllActions || flags.rawValue != 0 ||
      temporaryMode) {
    state &= ~kActionTriggerStateModeMask;
    if (temporaryMode) {
      // The legacy 0xFFFFFFFF sentinel means all actions. In mode 6 encode
      // its currently defined actions so it cannot be mistaken for legacy.
      if (state == 0x3FFFFFFF) state = 0xFFFF;
      state |= kActionTriggerStateModeMask;
    } else if (flags.flags.weeklySchedule) {
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

  delete temporaryUnlock;
  temporaryUnlock = nullptr;
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "at_tmp");
    TemporaryUnlockConfig stored;
    if (cfg->getBlobSize(key) == sizeof(stored) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&stored), sizeof(stored)) &&
        stored.version == 1 &&
        stored.function == SUPLA_CHANNELFNC_ACTIONTRIGGER &&
        stored.duration <= kMaximumTemporaryUnlockDurationS &&
        stored.edge <= SUPLA_AT_SENSOR_EDGE_BOTH &&
        (stored.escapedActions == 0 ||
         (stored.escapedActions & ~kActionTriggerStateModeMask) ==
             0x3FFFFFFFUL) &&
        ensureTemporaryUnlockState()) {
      temporaryUnlock->config = stored;
      generateKey(key, "at_actions");
      if (cfg->getUInt32(key, &lastReceivedActiveActions)) {
        temporaryUnlock->hasStoredActiveActions = true;
        activeActionsFromServer = lastReceivedActiveActions;
      }
    }
  }
  syncEffectiveButtonLock();

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
      runtimeFlags.localUnlockAllowed = localUnlockValue != 0;
    } else {
      runtimeFlags.localUnlockAllowed = false;
    }
    loadConfigChangeFlag();
  }
  setLocalUnlockAllowed(runtimeFlags.localUnlockAllowed);

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
    runtimeFlags.channelConfigReceived = false;
  }
}

void Supla::Control::ActionTrigger::onLoadState() {
  if (temporaryUnlock) {
    temporaryUnlock->durationMs = 0;
    temporaryUnlock->baselineKnown = false;
  }
  syncEffectiveButtonLock();
  if (!runtimeFlags.storageEnabled) {
    return;
  }

  runtimeFlags.channelConfigReceived = false;

  uint32_t state = activeActionsFromServer;
  const bool stateLoaded = Supla::Storage::ReadState(
      reinterpret_cast<unsigned char *>(&state), sizeof(state));

  ActionTriggerFlags flags = {};
  if (!stateLoaded) {
    return;
  }

  bool temporaryMode = false;
  flags.rawValue = 0;
  if (state == kActionTriggerLegacyAllActions) {
    // Keep compatibility with the old all-actions sentinel, which predates
    // the two mode bits.
    activeActionsFromServer = state;
  } else {
    switch (state & kActionTriggerStateModeMask) {
      case kActionTriggerStateModeMask:
        temporaryMode = true;
        break;
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
    if (temporaryUnlock && temporaryUnlock->config.escapedActions &&
        ((temporaryMode && activeActionsFromServer == 0xFFFF) ||
         (flags.flags.weeklySchedule &&
          activeActionsFromServer == 0x3FFFFFFFUL))) {
      activeActionsFromServer = temporaryUnlock->config.escapedActions;
    }
  }

  if (temporaryUnlock && temporaryUnlock->hasStoredActiveActions) {
    // The state bitmap can contain a publish-all routing override or an older
    // asynchronously saved selection. The durable config remains authoritative.
    activeActionsFromServer = lastReceivedActiveActions;
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
    if (temporaryMode) ensureTemporaryUnlockState();
    applyButtonMode(temporaryMode
        ? SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED
        : flags.flags.locked ? SUPLA_BUTTON_MODE_LOCKED
                                       : SUPLA_BUTTON_MODE_NOT_SET);
  }
}

void Supla::Control::ActionTrigger::enableStateStorage() {
  runtimeFlags.storageEnabled = true;
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
  runtimeFlags.alwaysUseOnClick1 = true;
  synchronizeDirectionalButtonModes();
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setLocalUnlockAllowed(bool allowed) {
  runtimeFlags.localUnlockAllowed = allowed;
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        isEffectivelyLocked(),
        runtimeFlags.localUnlockAllowed,
        runtimeFlags.keepConfigButtonTriggerAlwaysAvailable);
  }
  return *this;
}

bool Supla::Control::ActionTrigger::isLocalUnlockAllowed() const {
  return runtimeFlags.localUnlockAllowed;
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setKeepConfigButtonTriggerAlwaysAvailable(
    bool keep) {
  runtimeFlags.keepConfigButtonTriggerAlwaysAvailable = keep;
  if (attachedButton != nullptr) {
    attachedButton->setActionTriggerModeLocked(
        isEffectivelyLocked(),
        runtimeFlags.localUnlockAllowed,
        runtimeFlags.keepConfigButtonTriggerAlwaysAvailable);
  }
  return *this;
}

bool Supla::Control::ActionTrigger::
    keepsConfigButtonTriggerAlwaysAvailable() const {
  return runtimeFlags.keepConfigButtonTriggerAlwaysAvailable;
}

void Supla::Control::ActionTrigger::enable() {
  runtimeFlags.enabled = true;
}

void Supla::Control::ActionTrigger::disable() {
  runtimeFlags.enabled = false;
}

bool Supla::Control::ActionTrigger::setWeeklyScheduleController(
    WeeklyScheduleController *controller,
    WeeklyScheduleConfigHandler *configHandler,
    WeeklyScheduleProgramSource *programSource) {
  if (!weeklyScheduleComponents.set(
          controller, configHandler, programSource)) {
    return false;
  }
  runtimeFlags.weeklyScheduleAvailable = true;
  updateWeeklyScheduleCapabilities();
  return true;
}

bool Supla::Control::ActionTrigger::ensureNativeWeeklyScheduleController() {
  if (!runtimeFlags.weeklyScheduleAvailable ||
      weeklyScheduleComponents.isAssigned()) {
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
      runtimeFlags.weeklyScheduleAvailable && configHandler != nullptr &&
      configHandler->supportsConfigType(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  if (configurable) {
    channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    usedConfigTypes.set(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  } else {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
    usedConfigTypes.clear(SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE);
  }
  if (!runtimeFlags.weeklyScheduleAvailable && controller != nullptr) {
    controller->switchToManualMode();
  }
}

bool Supla::Control::ActionTrigger::isWeeklyScheduleSupported() const {
  return runtimeFlags.weeklyScheduleAvailable;
}

Supla::Control::ActionTrigger &
Supla::Control::ActionTrigger::setWeeklyScheduleAvailable(bool available) {
  runtimeFlags.weeklyScheduleAvailable = available;
  if (available) {
    ensureNativeWeeklyScheduleController();
  }
  updateWeeklyScheduleCapabilities();
  return *this;
}

void Supla::Control::ActionTrigger::fillDefaultWeeklySchedule(
    TChannelConfig_WeeklySchedule *schedule) {
  if (schedule == nullptr) {
    return;
  }

  schedule->Program[0].Mode = SUPLA_BUTTON_MODE_NOT_SET;
  schedule->Program[1].Mode = SUPLA_BUTTON_MODE_LOCKED;
}

bool Supla::Control::ActionTrigger::isWeeklyScheduleProgramModeSupported(
    uint8_t mode) const {
  return mode == SUPLA_BUTTON_MODE_NOT_SET ||
         mode == SUPLA_BUTTON_MODE_LOCKED ||
         mode == SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED;
}

void Supla::Control::ActionTrigger::iterateAlways() {
  Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr && isWeeklyScheduleSupported()) {
    weeklySchedule->processWeeklySchedule();
  }
  // Sample edges before retiring an expired timer. An edge that arrived
  // between timer iterations must not revive the just-expired unlock.
  updateUnlockSensor();
  updateTemporaryUnlock();
}

int32_t Supla::Control::ActionTrigger::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  if (newValue == nullptr) {
    return -1;
  }
  Supla::AutoLock lock(SuplaDevice.getTimerAccessMutex());
  auto *properties =
      reinterpret_cast<TActionTriggerProperties *>(newValue->value);
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  switch (properties->ButtonMode) {
    case SUPLA_BUTTON_MODE_CMD_TEMPORARY_UNLOCK:
      return startTemporaryUnlock(newValue->DurationSec) ? 1 : 0;
    case SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED:
      if (!prepareTemporaryUnlockState()) return 0;
      applyManualButtonMode(properties->ButtonMode);
      return 1;
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
  if (channelConfig && configType == SUPLA_CONFIG_TYPE_DEFAULT) {
    TChannelConfig_ActionTrigger wire = {};
    // Publish-all mode overrides only runtime routing, not Cloud's selection.
    wire.ActiveActions = (runtimeFlags.channelConfigReceived ||
        (temporaryUnlock && temporaryUnlock->hasStoredActiveActions))
        ? lastReceivedActiveActions : activeActionsFromServer;
    const auto config = currentTemporaryUnlockConfig();
    wire.TemporaryUnlockDurationSec = config.duration;
    wire.TemporaryUnlockSensorEdge = config.edge;
    wire.TemporaryUnlockSensorIsSet = config.sensor != 255;
    wire.TemporaryUnlockSensorChannelNo = config.sensor != 255
        ? config.sensor : 0;
    memcpy(channelConfig, &wire, sizeof(wire));
    *size = sizeof(wire);
    return;
  }
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
  if (auto cfg = Supla::Storage::ConfigInstance()) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "at_tmp");
    cfg->eraseKey(key);
    generateKey(key, "at_actions");
    cfg->eraseKey(key);
  }
  delete temporaryUnlock;
  temporaryUnlock = nullptr;
  syncEffectiveButtonLock();
  ElementWithChannelActions::purgeConfig();
  auto *configHandler = weeklyScheduleComponents.getConfigHandler();
  if (configHandler != nullptr) {
    configHandler->purgeConfig();
  }
}

void Supla::Control::ActionTrigger::applyButtonMode(uint8_t mode) {
  if (temporaryUnlock) {
    if (mode != channel.getButtonMode()) temporaryUnlock->baselineKnown = false;
    if (mode != SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED) {
      temporaryUnlock->durationMs = 0;
    }
  }
  channel.setButtonModeAndTemporaryUnlock(mode, isTemporaryUnlockActive());
  syncEffectiveButtonLock();
}

void Supla::Control::ActionTrigger::applyManualButtonMode(uint8_t mode) {
  auto *weeklySchedule = weeklyScheduleComponents.getController();
  if (weeklySchedule != nullptr) {
    // Apply only the requested final button mode. The controller's manual
    // transition synchronizes mode 0, which would cancel a live 6 -> 6 timer.
    if (temporaryUnlock) temporaryUnlock->manualModeTransition = true;
    weeklySchedule->switchToManualMode();
    if (temporaryUnlock) temporaryUnlock->manualModeTransition = false;
  }
  channel.setWeeklyScheduleEnabled(false);
  if (temporaryUnlock) temporaryUnlock->weeklyDurationS = 0;
  applyButtonMode(mode);
  scheduleStateSave();
}

void Supla::Control::ActionTrigger::scheduleStateSave(uint32_t delayMsMax,
                                                       uint32_t delayMsMin) {
  if (runtimeFlags.storageEnabled) {
    Supla::Storage::ScheduleSave(delayMsMax, delayMsMin);
  }
}


bool Supla::Control::ActionTrigger::ensureTemporaryUnlockState() {
  if (temporaryUnlock) return true;
#ifndef ARDUINO_ARCH_AVR
  temporaryUnlock = new (std::nothrow) TemporaryUnlockState();
#else
  temporaryUnlock = new TemporaryUnlockState();
#endif
  return temporaryUnlock != nullptr;
}

bool Supla::Control::ActionTrigger::persistTemporaryUnlockConfig(
    const TemporaryUnlockConfig &config, bool localChange,
    const uint32_t *activeActions) {
  if (!ensureTemporaryUnlockState()) return false;
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) return true;
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "at_tmp");
  TemporaryUnlockConfig previous;
  const bool existed = cfg->getBlobSize(key) == sizeof(previous) &&
      cfg->getBlob(key, reinterpret_cast<char *>(&previous), sizeof(previous));
  const bool recordChanged = !existed ||
      memcmp(&config, &previous, sizeof(config)) != 0;
  char changedTypesKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  uint32_t previousTypes = 0;
  uint32_t pendingTypes = 0;
  bool typesExisted = false;
  bool typesChanged = false;
  char actionsKey[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  uint32_t previousActions = 0;
  bool actionsExisted = false;
  bool actionsChanged = false;
  if (activeActions) {
    generateKey(actionsKey, "at_actions");
    actionsExisted = cfg->getUInt32(actionsKey, &previousActions);
    actionsChanged = !actionsExisted || previousActions != *activeActions;
  }
  if (localChange) {
    generateKey(changedTypesKey,
                Supla::ConfigTag::ChannelConfigChangedTypesTag);
    typesExisted = cfg->getUInt32(changedTypesKey, &previousTypes);
    auto types = locallyChangedConfigTypes;
    types.set(SUPLA_CONFIG_TYPE_DEFAULT);
    pendingTypes = types.getAll();
    typesChanged = !typesExisted || previousTypes != pendingTypes;
  }
  if (!temporaryUnlock->persistenceUncertain &&
      !recordChanged && !typesChanged && !actionsChanged) {
    if (activeActions) temporaryUnlock->hasStoredActiveActions = true;
    return true;
  }
  // Persist settings, original actions and the pending marker together.
  // Only then may the caller publish either change to the running element.
  if ((!typesChanged || cfg->setUInt32(changedTypesKey, pendingTypes)) &&
      (!actionsChanged || cfg->setUInt32(actionsKey, *activeActions)) &&
      (!recordChanged || cfg->setBlob(key,
          reinterpret_cast<const char *>(&config), sizeof(config))) &&
      cfg->commitWithResult()) {
    TemporaryUnlockConfig confirmed;
    uint32_t confirmedTypes = 0;
    uint32_t confirmedActions = 0;
    if (cfg->getBlobSize(key) == sizeof(confirmed) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&confirmed),
                     sizeof(confirmed)) &&
        !memcmp(&config, &confirmed, sizeof(config)) &&
        (!activeActions || (cfg->getUInt32(actionsKey, &confirmedActions) &&
                            confirmedActions == *activeActions)) &&
        (!localChange || (cfg->getUInt32(changedTypesKey, &confirmedTypes) &&
                         confirmedTypes == pendingTypes))) {
      temporaryUnlock->persistenceUncertain = false;
      if (activeActions) temporaryUnlock->hasStoredActiveActions = true;
      return true;
    }
  }
  temporaryUnlock->persistenceUncertain = true;
  if (actionsChanged) {
    if (actionsExisted) {
      cfg->setUInt32(actionsKey, previousActions);
    } else {
      cfg->eraseKey(actionsKey);
    }
  }
  if (typesChanged) {
    if (typesExisted) {
      cfg->setUInt32(changedTypesKey, previousTypes);
    } else {
      cfg->eraseKey(changedTypesKey);
    }
  }
  if (existed) {
    cfg->setBlob(key, reinterpret_cast<const char *>(&previous),
                 sizeof(previous));
  } else {
    cfg->eraseKey(key);
  }
  return false;
}

bool Supla::Control::ActionTrigger::prepareTemporaryUnlockState() {
  if (!ensureTemporaryUnlockState()) return false;
  auto config = temporaryUnlock->config;
  config.escapedActions =
      (activeActionsFromServer & ~kActionTriggerStateModeMask) == 0x3FFFFFFFUL
          ? activeActionsFromServer : 0;
  if (config.escapedActions != temporaryUnlock->config.escapedActions ||
      temporaryUnlock->persistenceUncertain) {
    if (!persistTemporaryUnlockConfig(config)) return false;
    temporaryUnlock->config = config;
  }
  return true;
}

Supla::Control::ActionTrigger::TemporaryUnlockConfig
Supla::Control::ActionTrigger::currentTemporaryUnlockConfig() const {
  return temporaryUnlock ? temporaryUnlock->config : TemporaryUnlockConfig{};
}

bool Supla::Control::ActionTrigger::isTemporaryUnlockActive() const {
  return temporaryUnlock && temporaryUnlock->durationMs != 0 &&
      static_cast<uint32_t>(millis() - temporaryUnlock->startMs) <
          temporaryUnlock->durationMs;
}

bool Supla::Control::ActionTrigger::isEffectivelyLocked() const {
  return channel.getButtonMode() == SUPLA_BUTTON_MODE_LOCKED ||
      (channel.getButtonMode() ==
           SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED &&
       !isTemporaryUnlockActive());
}

void Supla::Control::ActionTrigger::syncEffectiveButtonLock() {
  channel.setTemporaryUnlockActive(isTemporaryUnlockActive());
  if (attachedButton) {
    attachedButton->setTemporaryUnlockDeadline(
        temporaryUnlock ? temporaryUnlock->startMs : 0,
        isTemporaryUnlockActive() ? temporaryUnlock->durationMs : 0);
    attachedButton->setActionTriggerModeLocked(isEffectivelyLocked(),
        runtimeFlags.localUnlockAllowed,
        runtimeFlags.keepConfigButtonTriggerAlwaysAvailable);
  }
}

bool Supla::Control::ActionTrigger::startTemporaryUnlock(uint32_t seconds) {
  if (channel.getButtonMode() !=
      SUPLA_BUTTON_MODE_LOCKED_TEMPORARY_UNLOCK_ALLOWED ||
      seconds > kMaximumTemporaryUnlockDurationS ||
      !ensureTemporaryUnlockState()) return false;
  if (seconds == 0) seconds = temporaryUnlock->weeklyDurationS
      ? temporaryUnlock->weeklyDurationS : temporaryUnlock->config.duration;
  if (seconds == 0) return false;
  temporaryUnlock->startMs = millis();
  temporaryUnlock->durationMs = seconds * 1000;
  syncEffectiveButtonLock();
  return true;
}

void Supla::Control::ActionTrigger::updateTemporaryUnlock() {
  if (temporaryUnlock && temporaryUnlock->durationMs &&
      !isTemporaryUnlockActive()) {
    temporaryUnlock->durationMs = 0;
    // A transition may have arrived during the timer but not yet been read.
    temporaryUnlock->baselineKnown = false;
    syncEffectiveButtonLock();
  }
}

void Supla::Control::ActionTrigger::updateUnlockSensor() {
  if (!temporaryUnlock ||
      temporaryUnlock->config.edge == SUPLA_AT_SENSOR_EDGE_NONE) return;
  const auto sensor = Supla::Channel::GetByChannelNumber(
      temporaryUnlock->config.sensor);
  if (!sensor || !sensor->isStateOnline() ||
      sensor->isStateOnlineAndNotAvailable() ||
      sensor->isStateFirmwareUpdateOngoing() ||
      sensor->getChannelType() != SUPLA_CHANNELTYPE_BINARYSENSOR) {
    temporaryUnlock->baselineKnown = false;
    return;
  }
  const bool value = sensor->getValueBool();
  const bool edge = temporaryUnlock->baselineKnown &&
      value != temporaryUnlock->baseline &&
      (temporaryUnlock->config.edge == SUPLA_AT_SENSOR_EDGE_BOTH ||
       (value && temporaryUnlock->config.edge == SUPLA_AT_SENSOR_EDGE_RISING) ||
       (!value &&
        temporaryUnlock->config.edge == SUPLA_AT_SENSOR_EDGE_FALLING));
  temporaryUnlock->baselineKnown = true;
  temporaryUnlock->baseline = value;
  if (edge && temporaryUnlock->durationMs == 0) startTemporaryUnlock(0);
}
