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

#include "general_purpose_channel_base.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <supla/time.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>

#include "measurement_driver.h"
#include "memory_variable_driver.h"

using Supla::Sensor::GeneralPurposeChannelBase;

GeneralPurposeChannelBase::GeneralPurposeChannelBase(MeasurementDriver *driver,
    bool addMemoryVariableDriver)
    : driver(driver) {
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);

  if (this->driver == nullptr && addMemoryVariableDriver) {
    this->driver = new MemoryVariableDriver;
    deleteDriver = true;
  }
}

GeneralPurposeChannelBase::~GeneralPurposeChannelBase() {
  if (deleteDriver && driver) {
    delete driver;
    driver = nullptr;
    deleteDriver = false;
  }
}

double GeneralPurposeChannelBase::getCalculatedValue() {
  double channelRawValue = getChannel()->getValueDouble();
  if (isnan(channelRawValue)) {
    return channelRawValue;
  }

  double valueMultiplier = 1;
  double valueDivider = 1;
  if (getValueMultiplier() != 0) {
    valueMultiplier = getValueMultiplier() / 1000.0;
  }
  if (getValueDivider() != 0) {
    valueDivider = getValueDivider() / 1000.0;
  }
  double valueAdded = getValueAdded() / 1000.0;

  return channelRawValue * valueMultiplier / valueDivider + valueAdded;
}

void GeneralPurposeChannelBase::getFormattedValue(char *result, int maxSize) {
  if (result == nullptr) {
    return;
  }
  char unitBefore[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
  getUnitBeforeValue(unitBefore);
  char unitAfter[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
  getUnitAfterValue(unitAfter);
  uint8_t precision = getValuePrecision();
  char space[2] = " ";
  char noSpace[1] = "";
  bool noSpaceBeforeValue = getNoSpaceBeforeValue();
  bool noSpaceAfterValue = getNoSpaceAfterValue();
  if (unitBefore[0] == '\0') {
    noSpaceBeforeValue = true;
  }
  if (unitAfter[0] == '\0') {
    noSpaceAfterValue = true;
  }

  double calculatedValue = getCalculatedValue();

  if (isnan(calculatedValue)) {
    snprintf(result,
        maxSize,
        "%s%s---%s%s",
        unitBefore,
        noSpaceBeforeValue ? noSpace : space,
        noSpaceAfterValue ? noSpace : space,
        unitAfter);
  } else {
    snprintf(result,
        maxSize,
        "%s%s%.*f%s%s",
        unitBefore,
        noSpaceBeforeValue ? noSpace : space,
        precision,
        getCalculatedValue(),
        noSpaceAfterValue ? noSpace : space,
        unitAfter);
  }
}

void GeneralPurposeChannelBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  bool configLoaded = false;
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, "gpm_common");
    if (cfg->getBlob(key,
          reinterpret_cast<char *>(&commonConfig),
          sizeof(GPMCommonConfig))) {
      SUPLA_LOG_INFO("GPM[%d]: config loaded successfully", getChannelNumber());
      configLoaded = true;
    }

    // load config changed offline flags
    loadConfigChangeFlag();
    lastReadTime = 0;
  } else {
    SUPLA_LOG_WARNING("GPM[%d]: Failed to load config", getChannelNumber());
  }

  if (!configLoaded) {
    setValueAdded(getDefaultValueAdded(), false);
    setValueMultiplier(getDefaultValueMultiplier(), false);
    setValueDivider(getDefaultValueDivider(), false);
    setValuePrecision(getDefaultValuePrecision(), false);
    char unit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE] = {};
    getDefaultUnitBeforeValue(unit);
    setUnitBeforeValue(unit, false);
    getDefaultUnitAfterValue(unit);
    setUnitAfterValue(unit, false);
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
  return;
}

double GeneralPurposeChannelBase::getValue() {
  if (driver) {
    return driver->getValue();
  }
  return NAN;
}

void GeneralPurposeChannelBase::onInit() {
  if (driver) {
    driver->initialize();
  }
  channel.setNewValue(getValue());
}

void GeneralPurposeChannelBase::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

void GeneralPurposeChannelBase::setDefaultValueDivider(int32_t divider) {
  SUPLA_LOG_DEBUG(
      "GPM[%d]: DefaultValueDivider %d", getChannelNumber(), divider);
  defaultValueDivider = divider;
}

void GeneralPurposeChannelBase::setDefaultValueMultiplier(int32_t multiplier) {
  SUPLA_LOG_DEBUG(
      "GPM[%d]: DefaultValueMultiplier %d", getChannelNumber(),
      multiplier);
  defaultValueMultiplier = multiplier;
}

void GeneralPurposeChannelBase::setDefaultValueAdded(int64_t added) {
  SUPLA_LOG_DEBUG("GPM[%d]: DefaultValueAdded %lld", getChannelNumber(), added);
  defaultValueAdded = added;
}

void GeneralPurposeChannelBase::setDefaultValuePrecision(uint8_t precision) {
  SUPLA_LOG_DEBUG(
      "GPM[%d]: DefaultValuePrecision %d", getChannelNumber(),
      precision);
  defaultValuePrecision = precision;
}

void GeneralPurposeChannelBase::setDefaultUnitBeforeValue(const char *unit) {
  if (unit) {
    SUPLA_LOG_DEBUG(
        "GPM[%d]: DefaultUnitBeforeValue \"%s\"", getChannelNumber(), unit);
    strncpy(defaultUnitBeforeValue, unit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

void GeneralPurposeChannelBase::setDefaultUnitAfterValue(const char *unit) {
  if (unit) {
    SUPLA_LOG_DEBUG(
        "GPM[%d]: DefaultUnitAfterValue \"%s\"", getChannelNumber(), unit);
    strncpy(defaultUnitAfterValue, unit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

int32_t GeneralPurposeChannelBase::getDefaultValueDivider() const {
  return defaultValueDivider;
}

int32_t GeneralPurposeChannelBase::getDefaultValueMultiplier() const {
  return defaultValueMultiplier;
}

int64_t GeneralPurposeChannelBase::getDefaultValueAdded() const {
  return defaultValueAdded;
}

uint8_t GeneralPurposeChannelBase::getDefaultValuePrecision() const {
  return defaultValuePrecision;
}

void GeneralPurposeChannelBase::getDefaultUnitBeforeValue(char *unit) {
  if (unit) {
    strncpy(unit, defaultUnitBeforeValue, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

void GeneralPurposeChannelBase::getDefaultUnitAfterValue(char *unit) {
  if (unit) {
    strncpy(unit, defaultUnitAfterValue, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

int32_t GeneralPurposeChannelBase::getValueDivider() const {
  return commonConfig.divider;
}

int32_t GeneralPurposeChannelBase::getValueMultiplier() const {
  return commonConfig.multiplier;
}

int64_t GeneralPurposeChannelBase::getValueAdded() const {
  return commonConfig.added;
}

uint8_t GeneralPurposeChannelBase::getValuePrecision() const {
  return commonConfig.precision;
}

void GeneralPurposeChannelBase::getUnitBeforeValue(char *unit) const {
  if (unit) {
    strncpy(
        unit, commonConfig.unitBeforeValue, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

void GeneralPurposeChannelBase::getUnitAfterValue(char *unit) const {
  if (unit) {
    strncpy(unit, commonConfig.unitAfterValue, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
  }
}

uint8_t GeneralPurposeChannelBase::getNoSpaceBeforeValue() const {
  return commonConfig.noSpaceBeforeValue;
}

uint8_t GeneralPurposeChannelBase::getNoSpaceAfterValue() const {
  return commonConfig.noSpaceAfterValue;
}

uint8_t GeneralPurposeChannelBase::getKeepHistory() const {
  return commonConfig.keepHistory;
}

uint8_t GeneralPurposeChannelBase::getChartType() const {
  return commonConfig.chartType;
}

uint16_t GeneralPurposeChannelBase::getRefreshIntervalMs() const {
  return commonConfig.refreshIntervalMs;
}

void GeneralPurposeChannelBase::setChannelRefreshIntervalMs(
    uint16_t intervalMs) {
  if (intervalMs == 0) {
    intervalMs = 5000;
  } else if (intervalMs < 200) {
    intervalMs = 200;
  }
  refreshIntervalMs = intervalMs;
}

void GeneralPurposeChannelBase::setRefreshIntervalMs(int intervalMs,
                                                     bool local) {
  // RefreshIntervalMs in config contain value from server/Cloud. We don't
  // adjust it to internal value.
  if (intervalMs < 0) {
    intervalMs = 0;
  } else if (intervalMs > 65535) {
    intervalMs = 65535;
  }
  auto oldIntervalMs = getRefreshIntervalMs();
  commonConfig.refreshIntervalMs = intervalMs;
  if (intervalMs != oldIntervalMs && local) {
    setChannelRefreshIntervalMs(intervalMs);
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setValueDivider(int32_t divider, bool local) {
  auto oldDivider = getValueDivider();
  commonConfig.divider = divider;
  if (divider != oldDivider && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setValueMultiplier(int32_t multiplier,
                                                   bool local) {
  auto oldMultiplier = getValueMultiplier();
  commonConfig.multiplier = multiplier;
  if (multiplier != oldMultiplier && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setValueAdded(int64_t added, bool local) {
  auto oldAdded = getValueAdded();
  commonConfig.added = added;
  if (added != oldAdded && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setValuePrecision(uint8_t precision,
                                                  bool local) {
  auto oldPrecision = getValuePrecision();
  commonConfig.precision = precision;
  if (precision != oldPrecision && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setUnitBeforeValue(const char *unit,
                                                   bool local) {
  char oldUnit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE];
  getUnitBeforeValue(oldUnit);

  if (unit && strncmp(unit, oldUnit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE) != 0) {
    strncpy(
        commonConfig.unitBeforeValue, unit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
    if (local) {
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      saveConfig();
      saveConfigChangeFlag();
    }
  }
}

void GeneralPurposeChannelBase::setUnitAfterValue(const char *unit,
                                                  bool local) {
  char oldUnit[SUPLA_GENERAL_PURPOSE_UNIT_SIZE];
  getUnitAfterValue(oldUnit);

  if (unit && strncmp(unit, oldUnit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE) != 0) {
    strncpy(commonConfig.unitAfterValue, unit, SUPLA_GENERAL_PURPOSE_UNIT_SIZE);
    if (local) {
      channelConfigState = Supla::ChannelConfigState::LocalChangePending;
      saveConfig();
      saveConfigChangeFlag();
    }
  }
}

void GeneralPurposeChannelBase::setNoSpaceBeforeValue(
    uint8_t noSpaceBeforeValue, bool local) {
  auto oldNoSpaceBeforeValue = getNoSpaceBeforeValue();
  commonConfig.noSpaceBeforeValue = noSpaceBeforeValue;
  if (noSpaceBeforeValue != oldNoSpaceBeforeValue && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setNoSpaceAfterValue(uint8_t noSpaceAfterValue,
                                                     bool local) {
  auto oldNoSpaceAfterValue = getNoSpaceAfterValue();
  commonConfig.noSpaceAfterValue = noSpaceAfterValue;
  if (noSpaceAfterValue != oldNoSpaceAfterValue && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setKeepHistory(uint8_t keepHistory,
                                               bool local) {
  auto oldKeepHistory = getKeepHistory();
  commonConfig.keepHistory = keepHistory;
  if (keepHistory != oldKeepHistory && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

void GeneralPurposeChannelBase::setChartType(uint8_t chartType, bool local) {
  auto oldChartType = getChartType();
  commonConfig.chartType = chartType;
  if (chartType != oldChartType && local) {
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfig();
    saveConfigChangeFlag();
  }
}

uint8_t GeneralPurposeChannelBase::applyChannelConfig(
    TSD_ChannelConfig *result) {
  if (result == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }
  if (result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }
  if (result->ConfigSize != sizeof(TChannelConfig_GeneralPurposeMeasurement)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto config = reinterpret_cast<TChannelConfig_GeneralPurposeMeasurement *>(
      result->Config);

  GPMCommonConfig oldConfig = {};
  memcpy(&oldConfig, &commonConfig, sizeof(commonConfig));

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
  setRefreshIntervalMs(config->RefreshIntervalMs, false);

  if (memcmp(&oldConfig, &commonConfig, sizeof(commonConfig)) != 0) {
    saveConfig();
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
    channelConfigState = Supla::ChannelConfigState::LocalChangePending;
    saveConfigChangeFlag();
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}

void GeneralPurposeChannelBase::fillChannelConfig(void *channelConfig,
                                                  int *size) {
  if (size) {
    *size = 0;
  } else {
    return;
  }
  if (channelConfig == nullptr) {
    return;
  }

  auto *config = reinterpret_cast<TChannelConfig_GeneralPurposeMeasurement *>(
      channelConfig);
  *size = sizeof(TChannelConfig_GeneralPurposeMeasurement);

  config->ValueDivider = getValueDivider();
  config->ValueMultiplier = getValueMultiplier();
  config->ValueAdded = getValueAdded();
  config->ValuePrecision = getValuePrecision();
  config->NoSpaceBeforeValue = getNoSpaceBeforeValue();
  config->NoSpaceAfterValue = getNoSpaceAfterValue();
  config->KeepHistory = getKeepHistory();
  config->ChartType = getChartType();
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

void GeneralPurposeChannelBase::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    SUPLA_LOG_WARNING("GPM[%d]: Failed to save config", getChannelNumber());
    return;
  }

  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, "gpm_common");
  if (cfg->setBlob(key,
        reinterpret_cast<char *>(&commonConfig),
        sizeof(GPMCommonConfig))) {
    SUPLA_LOG_INFO("GPM[%d]: common config saved successfully",
                   getChannelNumber());
    cfg->saveWithDelay(5000);
  }

  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void GeneralPurposeChannelBase::setValue(const double &value) {
  if (driver) {
    driver->setValue(value);
  }
}
