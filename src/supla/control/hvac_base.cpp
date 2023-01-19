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

#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/log_wrapper.h>

using Supla::Control::HvacBase;

HvacBase::HvacBase() {
  channel.setType(SUPLA_CHANNELTYPE_HVAC);
  channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);

  // default modes of HVAC channel
  channel.setFuncList(SUPLA_HVAC_CAP_FLAG_MODE_HEAT |
                      SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  // default function is set in onInit based on supported modes or loaded from
  // config
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

void HvacBase::onLoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};

    // Read last set channel function
    Supla::Config::generateKey(key, getChannelNumber(), "hvac_fnc");
    int32_t channelFunc = 0;
    if (cfg->getInt32(key, &channelFunc)) {
      SUPLA_LOG_INFO("HVAC channel function loaded successfully (%d)",
                     channelFunc);
      channel.setDefault(channelFunc);
    } else {
      SUPLA_LOG_INFO("HVAC channel function missing. Using SW defaults");
    }

    // Generic HVAC configuration
    Supla::Config::generateKey(key, getChannelNumber(), "hvac_cfg");
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&config),
                     sizeof(TSD_ChannelConfig_HVAC))) {
      SUPLA_LOG_INFO("HVAC config loaded successfully");
    } else {
      SUPLA_LOG_INFO("HVAC config missing. Using SW defaults");
    }

    // Weekly schedule configuration
    Supla::Config::generateKey(key, getChannelNumber(), "hvac_weekly");
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&weeklySchedule),
                     sizeof(TSD_ChannelConfig_WeeklySchedule))) {
      SUPLA_LOG_INFO("HVAC weekly schedule loaded successfully");
      isWeeklyScheduleConfigured = true;
    } else {
      SUPLA_LOG_INFO("HVAC weekly schedule missing. Using SW defaults");
      isWeeklyScheduleConfigured = false;
    }
  } else {
    SUPLA_LOG_ERROR("HVAC can't work without config storage");
  }
}

void HvacBase::onLoadState() {
  if (getChannel() && getChannelNumber() >= 0) {
    auto hvacValue = getChannel()->getValueHvac();
    Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(hvacValue),
                            sizeof(THVACValue));
  }
}

void HvacBase::onSaveState() {
  if (getChannel() && getChannelNumber() >= 0) {
    auto hvacValue = getChannel()->getValueHvac();
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(hvacValue), sizeof(THVACValue));
  }
}

void HvacBase::onInit() {
  // init default channel function when it wasn't read from config or
  // set by user
  if (getChannel()->getDefaultFunction() == 0) {
    // set default to auto when both heat and cool are supported
    if (isHeatingSupported() && isCoolingSupported() && isAutoSupported()) {
      getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_AUTO);
    } else if (isHeatingSupported()) {
      getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT);
    } else if (isCoolingSupported()) {
      getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);
    } else if (isDrySupported()) {
      getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_DRYER);
    } else if (isFanSupported()) {
      getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_FAN);
    }
  }
}


void HvacBase::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  (void)(suplaSrpc);
  // Request temperature configuration from server
  // It will be received in onChannelConfigReceived
  // and saved in config storage
  requestTemperatureConfigFromServer = true;
}

void HvacBase::iterateAlways() {
}

void HvacBase::setHeatingSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_HEAT);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_HEAT);
  }
}

void HvacBase::setCoolingSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_COOL);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_COOL);
  }
}

void HvacBase::setAutoSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
    setHeatingSupported(true);
    setCoolingSupported(true);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_AUTO);
  }
}

void HvacBase::setFanSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_FAN);
  }
}

void HvacBase::setDrySupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  } else {
    channel.setFuncList(channel.getFuncList() & ~SUPLA_HVAC_CAP_FLAG_MODE_DRY);
  }
}

void HvacBase::setOnOffSupported(bool supported) {
  if (supported) {
    channel.setFuncList(channel.getFuncList() | SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  } else {
    channel.setFuncList(channel.getFuncList() &
                        ~SUPLA_HVAC_CAP_FLAG_MODE_ONOFF);
  }
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

