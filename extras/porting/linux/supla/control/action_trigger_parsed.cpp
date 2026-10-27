// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_trigger_parsed.h"

#include <supla/sensor/sensor_parsed.h>
#include <supla/log_wrapper.h>
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
