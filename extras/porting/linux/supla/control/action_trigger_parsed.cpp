/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "action_trigger_parsed.h"

#include <supla/sensor/sensor_parsed.h>
#include <supla/log_wrapper.h>

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
