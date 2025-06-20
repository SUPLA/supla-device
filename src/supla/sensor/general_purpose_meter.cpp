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

#include "general_purpose_meter.h"

#include <supla/sensor/measurement_driver.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/log_wrapper.h>
#include <supla/actions.h>
#include <supla/protocol/protocol_layer.h>
#include <string.h>
#include <math.h>

using Supla::Sensor::GeneralPurposeMeter;

GeneralPurposeMeter::GeneralPurposeMeter(
    Supla::Sensor::MeasurementDriver *driver, bool addMemoryVariableDriver)
    : GeneralPurposeChannelBase(driver, addMemoryVariableDriver) {
  channel.setType(SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_GENERAL_PURPOSE_METER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);
}

void GeneralPurposeMeter::onLoadConfig(SuplaDeviceClass *sdc) {
  GeneralPurposeChannelBase::onLoadConfig(sdc);

  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    SUPLA_LOG_WARNING("GPM[%d]: Failed to load config", getChannelNumber());
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "gpm_meter");
  if (cfg->getBlob(key,
        reinterpret_cast<char *>(&meterSpecificConfig),
        sizeof(GPMMeterSpecificConfig))) {
    SUPLA_LOG_INFO("GPM[%d]: meter config loaded successfully",
                   getChannelNumber());
  }
}

Supla::ApplyConfigResult GeneralPurposeMeter::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (result->ConfigSize == 0) {
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }
  if (result->ConfigSize != sizeof(TChannelConfig_GeneralPurposeMeter)) {
    return Supla::ApplyConfigResult::DataError;
  }

  auto config = reinterpret_cast<TChannelConfig_GeneralPurposeMeter *>(
      result->Config);

  GPMCommonConfig oldConfig = {};
  memcpy(&oldConfig, &commonConfig, sizeof(commonConfig));
  GPMMeterSpecificConfig oldMeterConfig = {};
  memcpy(&oldMeterConfig, &meterSpecificConfig, sizeof(meterSpecificConfig));

  setValueDivider(config->ValueDivider, false);
  setValueMultiplier(config->ValueMultiplier, false);
  setValueAdded(config->ValueAdded, false);
  setValuePrecision(config->ValuePrecision, false);
  setNoSpaceBeforeValue(config->NoSpaceBeforeValue, false);
  setNoSpaceAfterValue(config->NoSpaceAfterValue, false);
  setKeepHistory(config->KeepHistory, false);
  setChartType(config->ChartType, false);
  setUnitBeforeValue(config->UnitBeforeValue, false);
  setUnitAfterValue(config->UnitAfterValue, false);
  setCounterType(config->CounterType, false);
  setIncludeValueAddedInHistory(config->IncludeValueAddedInHistory, false);
  setFillMissingData(config->FillMissingData, false);
  setRefreshIntervalMs(config->RefreshIntervalMs, false);

  if (memcmp(&oldConfig, &commonConfig, sizeof(commonConfig)) != 0 ||
      memcmp(&oldMeterConfig, &meterSpecificConfig,
             sizeof(meterSpecificConfig)) != 0) {
    saveConfig();
    saveMeterSpecificConfig();
  }

  if (config->DefaultValueDivider != getDefaultValueDivider() ||
      config->DefaultValueMultiplier != getDefaultValueMultiplier() ||
      config->DefaultValueAdded != getDefaultValueAdded() ||
      config->DefaultValuePrecision != getDefaultValuePrecision() ||
      strncmp(config->DefaultUnitBeforeValue,
              defaultUnitBeforeValue,
              SUPLA_GENERAL_PURPOSE_UNIT_SIZE) ||
      strncmp(config->DefaultUnitAfterValue,
              defaultUnitAfterValue,
              SUPLA_GENERAL_PURPOSE_UNIT_SIZE)) {
    SUPLA_LOG_INFO("GPM[%d]: meter config changed", getChannelNumber());
    if (local) {
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      saveConfigChangeFlag();
    }

    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  return Supla::ApplyConfigResult::Success;
}

void GeneralPurposeMeter::fillChannelConfig(void *channelConfig,
                                            int *size,
                                            uint8_t configType) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  if (configType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return;
  }

  auto *config = reinterpret_cast<TChannelConfig_GeneralPurposeMeter *>(
      channelConfig);
  *size = sizeof(TChannelConfig_GeneralPurposeMeter);

  config->ValueDivider = getValueDivider();
  config->ValueMultiplier = getValueMultiplier();
  config->ValueAdded = getValueAdded();
  config->ValuePrecision = getValuePrecision();
  config->NoSpaceBeforeValue = getNoSpaceBeforeValue();
  config->NoSpaceAfterValue = getNoSpaceAfterValue();
  config->KeepHistory = getKeepHistory();
  config->ChartType = getChartType();
  config->CounterType = getCounterType();
  config->IncludeValueAddedInHistory = getIncludeValueAddedInHistory();
  config->FillMissingData = getFillMissingData();
  config->RefreshIntervalMs = getRefreshIntervalMs();

  config->DefaultValueDivider = getDefaultValueDivider();
  config->DefaultValueMultiplier = getDefaultValueMultiplier();
  config->DefaultValueAdded = getDefaultValueAdded();
  config->DefaultValuePrecision = getDefaultValuePrecision();

  getUnitBeforeValue(config->UnitBeforeValue);
  getUnitAfterValue(config->UnitAfterValue);
  getDefaultUnitBeforeValue(config->DefaultUnitBeforeValue);
  getDefaultUnitAfterValue(config->DefaultUnitAfterValue);
}

uint8_t GeneralPurposeMeter::getCounterType() const {
  return meterSpecificConfig.counterType;
}

uint8_t GeneralPurposeMeter::getIncludeValueAddedInHistory() const {
  return meterSpecificConfig.includeValueAddedInHistory;
}

uint8_t GeneralPurposeMeter::getFillMissingData() const {
  return meterSpecificConfig.fillMissingData;
}

void GeneralPurposeMeter::setCounterType(uint8_t counterType, bool local) {
  auto oldCounterType = getCounterType();
  meterSpecificConfig.counterType = counterType;
  if (counterType != oldCounterType && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveMeterSpecificConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeMeter::setIncludeValueAddedInHistory(
    uint8_t includeValueAddedInHistory, bool local) {
  auto oldIncludeValueAddedInHistory = getIncludeValueAddedInHistory();
  meterSpecificConfig.includeValueAddedInHistory = includeValueAddedInHistory;
  if (includeValueAddedInHistory != oldIncludeValueAddedInHistory && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveMeterSpecificConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeMeter::setFillMissingData(uint8_t fillMissingData,
                                             bool local) {
  auto oldFillMissingData = getFillMissingData();
  meterSpecificConfig.fillMissingData = fillMissingData;
  if (fillMissingData != oldFillMissingData && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveMeterSpecificConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeMeter::saveMeterSpecificConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    SUPLA_LOG_WARNING("GPM[%d]: Failed to save config", getChannelNumber());
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "gpm_meter");
  if (cfg->setBlob(key,
        reinterpret_cast<char *>(&meterSpecificConfig),
        sizeof(GPMMeterSpecificConfig))) {
    SUPLA_LOG_INFO("GPM[%d]: meter specific config saved successfully",
                   getChannelNumber());
    cfg->saveWithDelay(5000);
  }

  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void GeneralPurposeMeter::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case Supla::RESET: {
      setCounter(resetToValue);
      break;
    }
    case Supla::INCREMENT: {
      incCounter();
      break;
    }
    case Supla::DECREMENT: {
      decCounter();
      break;
    }
  }
}

int GeneralPurposeMeter::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request) {
    if (request->Command == SUPLA_CALCFG_CMD_RESET_COUNTERS) {
      if (!request->SuperUserAuthorized) {
        return SUPLA_CALCFG_RESULT_UNAUTHORIZED;
      }
      SUPLA_LOG_INFO("GPM[%d]: CALCFG reset counter received",
                     getChannelNumber());
      setCounter(resetToValue);
      return SUPLA_CALCFG_RESULT_DONE;
    }
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

void GeneralPurposeMeter::setCounter(double newValue) {
  setValue(newValue);
}

void GeneralPurposeMeter::incCounter(double incrementBy) {
  setValue(getValue() + (incrementBy == 0.0 ? valueStep : incrementBy));
}

void GeneralPurposeMeter::decCounter(double decrementBy) {
  setValue(getValue() - (decrementBy == 0.0 ? valueStep : decrementBy));
}

void GeneralPurposeMeter::setValueStep(double newValueStep) {
  if (!isnan(newValueStep)) {
    valueStep = newValueStep;
  }
}

void GeneralPurposeMeter::setResetToValue(double newResetToValue) {
  if (!isnan(newResetToValue)) {
    resetToValue = newResetToValue;
  }
}

void GeneralPurposeMeter::setCounterResetSupportFlag(bool support) {
  if (support) {
    channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);
  } else {
    channel.unsetFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);
  }
}

void GeneralPurposeMeter::setKeepStateInStorage(bool keep) {
  keepStateInStorage = keep;
}

void GeneralPurposeMeter::onSaveState() {
  if (keepStateInStorage) {
    double value = getValue();
    Supla::Storage::WriteState((unsigned char *)&value, sizeof(value));
  }
}

void GeneralPurposeMeter::onLoadState() {
  if (keepStateInStorage) {
    double value = 0.0;
    if (Supla::Storage::ReadState((unsigned char *)&value, sizeof(value))) {
      setCounter(value);
    }
  }
}
