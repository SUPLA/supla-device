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

#include "hvac_base.h"

using Supla::Control::HvacBase;

HvacBase::HvacBase() {
  channel.setType(SUPLA_CHANNELTYPE_HVAC);
  channel.setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);
  channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFuncList(SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                      SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
}

HvacBase::~HvacBase() {
}

bool HvacBase::iterateConnected() {
  auto result = Element::iterateConnected();
  if (result) {
    if (requestTemperatureConfigFromServer) {
      requestTemperatureConfigFromServer = false;
      result = false;
    }
  }

  return result;
}

bool HvacBase::isOnOffSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_ONOFF;
}

bool HvacBase::isHeatingSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_HEAT;
}

bool HvacBase::isCoolingSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_COOL;
}

bool HvacBase::isAutoSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_AUTO;
}

bool HvacBase::isFanSupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_FAN;
}

bool HvacBase::isDrySupported() {
  return channel.getFuncList() & SUPLA_HVAC_CAP_FLAG_MODE_DRY;
}

void HvacBase::onLoadConfig() {
}

void HvacBase::onLoadState() {
}

void HvacBase::onInit() {
}

void HvacBase::onSaveState() {
}

void HvacBase::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  (void)(suplaSrpc);
}

void HvacBase::iterateAlways() {
}
