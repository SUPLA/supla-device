// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_trigger_parsed.h"

#include <supla/sensor/sensor_parsed.h>
#include <supla/log_wrapper.h>
#include <cstring>
#include <string>

using Supla::Control::ActionTriggerParsed;

namespace {
int parsedActionId(int action) {
  if (action < 0 || action >= 32) {
    return -1;
  }
  return Supla::Control::ActionTrigger::actionTriggerCapToActionId(
      uint32_t{1} << action);
}
}  // namespace

ActionTriggerParsed::ActionTriggerParsed(const std::string &name) {
  Supla::Sensor::SensorParsedBase::registerAtName(name, this);
}

void ActionTriggerParsed::sendActionTrigger(int action) {
  const int actionId = parsedActionId(action);
  if (actionId >= 0) {
    ActionTrigger::handleAction(0, actionId);
  }
}

void ActionTriggerParsed::activateAction(int action) {
  const int actionId = parsedActionId(action);
  if (actionId >= 0) {
    ActionTrigger::activateAction(actionId);
  } else {
    SUPLA_LOG_WARNING("Invalid parsed ActionTrigger action %d", action);
  }
}

bool ActionTriggerParsed::setTemporaryUnlockDurationSec(uint32_t seconds) {
  if (seconds > MaximumTemporaryUnlockDurationSec) {
    return false;
  }
  temporaryUnlockDurationSec = seconds;
  return true;
}

void ActionTriggerParsed::onLoadConfig(SuplaDeviceClass *sdc) {
  ActionTrigger::onLoadConfig(sdc);
  if (!temporaryUnlockDurationSec.has_value()) {
    return;
  }

  TSD_ChannelConfig config = {};
  config.ChannelNumber = getChannelNumber();
  config.Func = SUPLA_CHANNELFNC_ACTIONTRIGGER;
  config.ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
  TChannelConfig_ActionTrigger value = {};
  int size = 0;
  fillChannelConfig(&value, &size, config.ConfigType);
  value.TemporaryUnlockDurationSec = *temporaryUnlockDurationSec;
  config.ConfigSize = size;
  memcpy(config.Config, &value, sizeof(value));
  if (handleChannelConfig(&config, true) != SUPLA_CONFIG_RESULT_TRUE) {
    SUPLA_LOG_ERROR("AT[%d]: failed to apply temporary_unlock_duration_sec",
                    getChannelNumber());
  }
}
