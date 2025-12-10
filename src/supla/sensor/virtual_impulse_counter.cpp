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

#include "virtual_impulse_counter.h"

#include <supla/actions.h>
#include <supla/log_wrapper.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/events.h>
#include <string.h>

using Supla::Sensor::VirtualImpulseCounter;

VirtualImpulseCounter::VirtualImpulseCounter() {
  channel.setType(SUPLA_CHANNELTYPE_IMPULSE_COUNTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_IC_WATER_METER);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

void VirtualImpulseCounter::onInit() {
}

uint64_t VirtualImpulseCounter::getCounter() const {
  return counter;
}

void VirtualImpulseCounter::onSaveState() {
  Supla::Storage::WriteState((unsigned char *)&counter, sizeof(counter));
}

void VirtualImpulseCounter::onLoadState() {
  uint64_t data = {};
  if (Supla::Storage::ReadState((unsigned char *)&data, sizeof(data))) {
    setCounter(data);
  }
}

void VirtualImpulseCounter::setCounter(uint64_t value) {
  counter = value;
  channel.setNewValue(value);
  SUPLA_LOG_DEBUG(
            "VirtualImpulseCounter[%d] - set counter to %d",
            channel.getChannelNumber(),
            static_cast<int>(counter));
}

void VirtualImpulseCounter::incCounter() {
  counter++;
  runAction(Supla::ON_IMPULSE);
}

void VirtualImpulseCounter::iterateAlways() {
  if (millis() - lastReadTime > 500) {
    lastReadTime = millis();
    if (forceStateSaveOnChange && channel.getValueInt64() != counter) {
      Supla::Storage::WriteStateStorage();
    }
    channel.setNewValue(counter);
  }
}

void VirtualImpulseCounter::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case RESET: {
      resetCounter();
      break;
    }
    case INCREMENT: {
      incCounter();
      break;
    }
  }
}

void VirtualImpulseCounter::resetCounter() {
  setCounter(0);
}

int VirtualImpulseCounter::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_RESET_COUNTERS) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      SUPLA_LOG_INFO("ImpulseCounter[%d] - CALCFG reset counter received",
                     channel.getChannelNumber());
      resetCounter();
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void VirtualImpulseCounter::setForceStateSaveOnChange(bool value) {
  forceStateSaveOnChange = value;
}

Supla::ApplyConfigResult VirtualImpulseCounter::applyChannelConfig(
    TSD_ChannelConfig *result, bool) {
  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      // Nothing to do
      return Supla::ApplyConfigResult::Success;
    }
  }

  return Supla::ApplyConfigResult::NotSupported;
}

void VirtualImpulseCounter::fillChannelConfig(void *channelConfig,
                                              int *size,
                                              uint8_t configType) {
  if (size && channelConfig) {
    if (configType == SUPLA_CONFIG_TYPE_DEFAULT) {
      // init default impulse counter config with 1000 impulses per unit
      *size = sizeof(TChannelConfig_ImpulseCounter);
      TChannelConfig_ImpulseCounter *config =
        reinterpret_cast<TChannelConfig_ImpulseCounter *>(channelConfig);
      memset(config, 0, sizeof(TChannelConfig_ImpulseCounter));
      config->ImpulsesPerUnit = 1000;
    }
  }
}
