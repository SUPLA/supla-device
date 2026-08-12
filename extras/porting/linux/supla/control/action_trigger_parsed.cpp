// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "action_trigger_parsed.h"

#include <supla/sensor/sensor_parsed.h>
#include <supla/log_wrapper.h>
#include <string>

using Supla::Control::ActionTriggerParsed;

ActionTriggerParsed::ActionTriggerParsed(const std::string &name) {
  Supla::Sensor::SensorParsedBase::registerAtName(name, this);
}

void ActionTriggerParsed::sendActionTrigger(int action) {
  uint32_t actionCap = (1 << action);

  if (actionCap & activeActionsFromServer ||
      actionHandlingType != ActionHandlingType_RelayOnSuplaServer) {
    channel.pushAction(actionCap);
  }
}

void ActionTriggerParsed::activateAction(int action) {
  SUPLA_LOG_INFO("Activating action %d", action);
  channel.activateAction((1 << action));
}
