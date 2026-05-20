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

#include "varilight_dimmer.h"

#include <supla-common/proto.h>
#include <supla/channels/channel.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/supla_srpc.h>

#include <algorithm>
#include <cstring>

namespace {

Supla::Control::Varilight::Configuration MakeDefaultConfig() {
  Supla::Control::Varilight::Configuration config = {};
  config.edgeMinimum = 0;
  config.edgeMaximum = 1000;
  config.operatingMinimum = 0;
  config.operatingMaximum = 1000;
  config.mode = 0;
  config.boost = 0;
  config.boostLevel = 0;
  config.childLock = 0;
  config.modeMask = 0;
  config.boostMask = 0;
  return config;
}

const char *CalcfgCommandToCStr(int32_t command) {
  switch (command) {
    case Supla::Control::Varilight::MsgRestoreDefaults:
      return "RESTORE_DEFAULTS";
    case Supla::Control::Varilight::MsgConfigurationMode:
      return "CONFIGURATION_MODE";
    case Supla::Control::Varilight::MsgSetMode:
      return "SET_MODE";
    case Supla::Control::Varilight::MsgSetMinimum:
      return "SET_MINIMUM";
    case Supla::Control::Varilight::MsgSetMaximum:
      return "SET_MAXIMUM";
    case Supla::Control::Varilight::MsgSetBoost:
      return "SET_BOOST";
    case Supla::Control::Varilight::MsgSetBoostLevel:
      return "SET_BOOST_LEVEL";
    case Supla::Control::Varilight::MsgConfigurationAck:
      return "CONFIGURATION_ACK";
    case Supla::Control::Varilight::MsgConfigurationReport:
      return "CONFIGURATION_REPORT";
    case Supla::Control::Varilight::MsgConfigComplete:
      return "CONFIG_COMPLETE";
    case Supla::Control::Varilight::CalcfgSetLedConfig:
      return "SET_LED_CONFIG";
    default:
      return "UNKNOWN";
  }
}

const char *CalcfgResultToCStr(int result) {
  switch (result) {
    case SUPLA_CALCFG_RESULT_UNAUTHORIZED:
      return "UNAUTHORIZED";
    case SUPLA_CALCFG_RESULT_NOT_SUPPORTED:
      return "NOT_SUPPORTED";
    case SUPLA_CALCFG_RESULT_DONE:
      return "DONE";
    case SUPLA_CALCFG_RESULT_TRUE:
      return "TRUE";
    case SUPLA_CALCFG_RESULT_FALSE:
      return "FALSE";
    case SUPLA_SRPC_CALCFG_RESULT_PENDING:
      return "PENDING";
    default:
      return "UNKNOWN";
  }
}

}  // namespace

Supla::Control::VarilightDimmer::VarilightDimmer(int channelNumber)
    : Supla::ChannelElement(channelNumber) {
  channel.setType(SUPLA_CHANNELTYPE_DIMMER);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMER);
  resetToDefaults();
}

void Supla::Control::VarilightDimmer::onRegistered(
    Supla::Protocol::SuplaSrpc *srpc) {
  suplaSrpc = srpc;
}

void Supla::Control::VarilightDimmer::iterateAlways() {
  if (configurationReportPending) {
    sendConfigurationReport();
  }
}

int32_t Supla::Control::VarilightDimmer::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  if (!newValue) {
    return 0;
  }

  uint8_t toggle = static_cast<uint8_t>(newValue->value[5]);
  uint8_t requestedBrightness = clampBrightness(
      static_cast<uint8_t>(newValue->value[0]));

  if (toggle && requestedBrightness == 0) {
    setBrightness(0);
  } else {
    setBrightness(requestedBrightness);
  }

  return -1;
}

int Supla::Control::VarilightDimmer::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (!request || request->SuperUserAuthorized != 1) {
    SUPLA_LOG_INFO("Varilight[%d] CALCFG unauthorized/invalid request",
                   getChannelNumber());
    return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
  }

  SUPLA_LOG_INFO("Varilight[%d] CALCFG received: %s (%d), dataSize=%d",
                 getChannelNumber(),
                 CalcfgCommandToCStr(request->Command),
                 request->Command,
                 request->DataSize);

  switch (request->Command) {
    case Varilight::MsgRestoreDefaults: {
      resetToDefaults();
      SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                     getChannelNumber(),
                     CalcfgResultToCStr(SUPLA_CALCFG_RESULT_DONE));
      return SUPLA_CALCFG_RESULT_DONE;
    }
    case Varilight::MsgConfigurationMode: {
      startConfiguration();
      configurationReportPending = true;
      SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s (%s)",
                     getChannelNumber(),
                     CalcfgResultToCStr(SUPLA_SRPC_CALCFG_RESULT_PENDING),
                     CalcfgCommandToCStr(request->Command));
      return SUPLA_SRPC_CALCFG_RESULT_PENDING;
    }
    case Varilight::MsgConfigComplete: {
      if (request->DataSize == 1) {
        completeConfiguration(request->Data[0] != 0);
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::MsgSetMinimum: {
      if (request->DataSize == 2) {
        currentConfig().operatingMinimum = clampLevel(
            readUint16(request->Data));
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::MsgSetMaximum: {
      if (request->DataSize == 2) {
        currentConfig().operatingMaximum = clampLevel(
            readUint16(request->Data));
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::MsgSetMode: {
      if (request->DataSize == 1) {
        currentConfig().mode = static_cast<uint8_t>(request->Data[0]);
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::MsgSetBoost: {
      if (request->DataSize == 1) {
        currentConfig().boost = static_cast<uint8_t>(request->Data[0]);
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::MsgSetBoostLevel: {
      if (request->DataSize == 2) {
        currentConfig().boostLevel = clampLevel(readUint16(request->Data));
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    case Varilight::CalcfgSetLedConfig: {
      if (request->DataSize == 1) {
        workingLedConfig = clampLed(static_cast<uint8_t>(request->Data[0]));
        if (!configurationActive) {
          ledConfig = workingLedConfig;
        }
        SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                       getChannelNumber(),
                       CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE));
        return SUPLA_CALCFG_RESULT_TRUE;
      }
      break;
    }
    default:
      SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                     getChannelNumber(),
                     CalcfgResultToCStr(SUPLA_CALCFG_RESULT_NOT_SUPPORTED));
      return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
  }

  SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s",
                 getChannelNumber(),
                 CalcfgResultToCStr(SUPLA_CALCFG_RESULT_DONE));
  return SUPLA_CALCFG_RESULT_DONE;
}

uint32_t Supla::Control::VarilightDimmer::getCalcfgPendingTimeoutMs(
    TSD_DeviceCalCfgRequest *request) const {
  if (!request || request->SuperUserAuthorized != 1) {
    return 0;
  }

  if (request->Command == Varilight::MsgConfigurationMode) {
    return 3000;
  }

  return 0;
}

void Supla::Control::VarilightDimmer::setBrightness(uint8_t value) {
  brightness = clampBrightness(value);
  channel.setNewValue(0, 0, 0, 0, brightness, 0);
}

uint8_t Supla::Control::VarilightDimmer::getBrightness() const {
  return brightness;
}

void Supla::Control::VarilightDimmer::setEdgeMinimum(uint16_t value) {
  config.edgeMinimum = clampLevel(value);
  workingConfig.edgeMinimum = config.edgeMinimum;
}

void Supla::Control::VarilightDimmer::setEdgeMaximum(uint16_t value) {
  config.edgeMaximum = clampLevel(value);
  workingConfig.edgeMaximum = config.edgeMaximum;
}

void Supla::Control::VarilightDimmer::setOperatingMinimum(uint16_t value) {
  config.operatingMinimum = clampLevel(value);
  workingConfig.operatingMinimum = config.operatingMinimum;
}

void Supla::Control::VarilightDimmer::setOperatingMaximum(uint16_t value) {
  config.operatingMaximum = clampLevel(value);
  workingConfig.operatingMaximum = config.operatingMaximum;
}

void Supla::Control::VarilightDimmer::setMode(uint8_t value) {
  config.mode = value;
  workingConfig.mode = value;
}

void Supla::Control::VarilightDimmer::setBoost(uint8_t value) {
  config.boost = value;
  workingConfig.boost = value;
}

void Supla::Control::VarilightDimmer::setBoostLevel(uint16_t value) {
  config.boostLevel = clampLevel(value);
  workingConfig.boostLevel = config.boostLevel;
}

void Supla::Control::VarilightDimmer::setChildLock(uint8_t value) {
  config.childLock = value;
  workingConfig.childLock = value;
}

void Supla::Control::VarilightDimmer::setModeMask(uint8_t value) {
  config.modeMask = value;
  workingConfig.modeMask = value;
}

void Supla::Control::VarilightDimmer::setBoostMask(uint8_t value) {
  config.boostMask = value;
  workingConfig.boostMask = value;
}

void Supla::Control::VarilightDimmer::setLedConfig(uint8_t value) {
  ledConfig = clampLed(value);
  workingLedConfig = ledConfig;
}

void Supla::Control::VarilightDimmer::setPicInstalledHexVersion(
    const char *value) {
  memset(picInstalledHexVer, 0, sizeof(picInstalledHexVer));
  if (value) {
    strncpy(picInstalledHexVer, value, sizeof(picInstalledHexVer) - 1);
  }
}

const Supla::Control::Varilight::Configuration &
Supla::Control::VarilightDimmer::getConfiguration() const {
  return config;
}

uint8_t Supla::Control::VarilightDimmer::getLedConfig() const {
  return ledConfig;
}

Supla::Control::Varilight::SuplaConfiguration
Supla::Control::VarilightDimmer::getSuplaConfiguration() const {
  Varilight::SuplaConfiguration suplaConfig = {};
  suplaConfig.mainConfig = currentConfig();
  suplaConfig.cfgVersion = Varilight::ConfigVersion;
  suplaConfig.led = configurationActive ? workingLedConfig : ledConfig;
  memcpy(suplaConfig.picInstalledHexVer,
         picInstalledHexVer,
         sizeof(suplaConfig.picInstalledHexVer));
  return suplaConfig;
}

uint16_t Supla::Control::VarilightDimmer::clampLevel(uint16_t value) {
  return std::min<uint16_t>(value, 1000);
}

uint8_t Supla::Control::VarilightDimmer::clampBrightness(uint8_t value) {
  return std::min<uint8_t>(value, 100);
}

uint8_t Supla::Control::VarilightDimmer::clampLed(uint8_t value) {
  return std::min<uint8_t>(value, 2);
}

uint16_t Supla::Control::VarilightDimmer::readUint16(const char *data) {
  return static_cast<uint16_t>(static_cast<uint8_t>(data[0])) |
         static_cast<uint16_t>(static_cast<uint8_t>(data[1]) << 8);
}

void Supla::Control::VarilightDimmer::resetToDefaults() {
  config = MakeDefaultConfig();
  workingConfig = config;
  ledConfig = Varilight::DefaultLedConfig;
  workingLedConfig = ledConfig;
  configurationActive = false;
  configurationReportPending = false;
}

void Supla::Control::VarilightDimmer::startConfiguration() {
  workingConfig = config;
  workingLedConfig = ledConfig;
  configurationActive = true;
}

Supla::Control::Varilight::Configuration &
Supla::Control::VarilightDimmer::currentConfig() {
  if (!configurationActive) {
    startConfiguration();
  }
  return workingConfig;
}

const Supla::Control::Varilight::Configuration &
Supla::Control::VarilightDimmer::currentConfig() const {
  return configurationActive ? workingConfig : config;
}

void Supla::Control::VarilightDimmer::sendConfigurationReport() {
  if (!suplaSrpc) {
    configurationReportPending = false;
    return;
  }

  SUPLA_LOG_INFO("Varilight[%d] CALCFG response: %s (%s)",
                 getChannelNumber(),
                 CalcfgResultToCStr(SUPLA_CALCFG_RESULT_TRUE),
                 CalcfgCommandToCStr(Varilight::MsgConfigurationReport));
  auto suplaConfig = getSuplaConfiguration();
  suplaSrpc->sendPendingCalCfgResultForCommand(
      getChannelNumber(),
      SUPLA_CALCFG_RESULT_TRUE,
      Varilight::MsgConfigurationMode,
      Varilight::MsgConfigurationReport,
      sizeof(suplaConfig),
      &suplaConfig);
  configurationReportPending = false;
}

void Supla::Control::VarilightDimmer::completeConfiguration(bool save) {
  SUPLA_LOG_INFO("Varilight[%d] CALCFG config complete: %s",
                 getChannelNumber(),
                 save ? "SAVE" : "CANCEL");
  if (save && configurationActive) {
    config = workingConfig;
    ledConfig = workingLedConfig;
  }

  workingConfig = config;
  workingLedConfig = ledConfig;
  configurationActive = false;
  configurationReportPending = false;

  if (suplaSrpc) {
    suplaSrpc->clearPendingCalCfgResult(getChannelNumber());
  }
}
