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

#include "remote_device_config.h"

#include <supla/element.h>
#include <supla/time.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/log_wrapper.h>
#include <supla/clock/clock.h>
#include <supla/storage/config_tags.h>
#include <supla/modbus/modbus_configurator.h>
#include <supla/device/auto_update_policy.h>

using Supla::Device::RemoteDeviceConfig;

uint64_t RemoteDeviceConfig::fieldBitsUsedByDevice = 0;
uint64_t RemoteDeviceConfig::homeScreenContentAvailable = 0;
Supla::Modbus::ConfigProperties RemoteDeviceConfig::modbusProperties;
uint8_t RemoteDeviceConfig::resendAttempts = 0;

RemoteDeviceConfig::RemoteDeviceConfig(bool firstDeviceConfigAfterRegistration)
    : firstDeviceConfigAfterRegistration(firstDeviceConfigAfterRegistration) {
}

RemoteDeviceConfig::~RemoteDeviceConfig() {
}

void RemoteDeviceConfig::ClearResendAttemptsCounter() {
  resendAttempts = 0;
}

void RemoteDeviceConfig::RegisterConfigField(uint64_t fieldBit) {
  if (fieldBit == 0 || (fieldBit & (fieldBit - 1)) != 0) {
    // (fieldBit & (fieldBit) - 1) will evaluate to 0 only when fieldBit had
    // only one bit set to 1 (that number was power of 2)
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid field 0x%X%08X",
                      PRINTF_UINT64_HEX(fieldBit));
    return;
  }
  if (!(fieldBitsUsedByDevice & fieldBit)) {
    SUPLA_LOG_INFO("RemoteDeviceConfig: Registering field 0x%X%08X",
                   PRINTF_UINT64_HEX(fieldBit));
  }
  fieldBitsUsedByDevice |= fieldBit;
}

void RemoteDeviceConfig::SetHomeScreenContentAvailable(uint64_t allValues) {
  if (allValues != homeScreenContentAvailable) {
    homeScreenContentAvailable = allValues;
    SUPLA_LOG_INFO("RemoteDeviceConfig: SetHomeScreenContentAvailable 0x%X%08X",
                   PRINTF_UINT64_HEX(homeScreenContentAvailable));
  }
}

uint64_t RemoteDeviceConfig::GetHomeScreenContentAvailable() {
  return homeScreenContentAvailable;
}

enum Supla::HomeScreenContent RemoteDeviceConfig::HomeScreenContentBitToEnum(
    uint64_t fieldBit) {
  if (fieldBit == 0 || (fieldBit & (fieldBit - 1)) != 0) {
    // (fieldBit & (fieldBit) - 1) will evaluate to 0 only when fieldBit had
    // only one bit set to 1 (that number was power of 2)
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid field 0x%X%08X",
                      PRINTF_UINT64_HEX(fieldBit));
    return Supla::HomeScreenContent::HOME_SCREEN_OFF;
  }

  switch (fieldBit) {
    default:
    case (1 << 0): {
      return Supla::HomeScreenContent::HOME_SCREEN_OFF;
    }
    case (1 << 1): {
      return Supla::HomeScreenContent::HOME_SCREEN_TEMPERATURE;
    }
    case (1 << 2): {
      return Supla::HomeScreenContent::HOME_SCREEN_TEMPERATURE_HUMIDITY;
    }
    case (1 << 3): {
      return Supla::HomeScreenContent::HOME_SCREEN_TIME;
    }
    case (1 << 4): {
      return Supla::HomeScreenContent::HOME_SCREEN_TIME_DATE;
    }
    case (1 << 5): {
      return Supla::HomeScreenContent::HOME_SCREEN_TEMPERATURE_TIME;
    }
    case (1 << 6): {
      return Supla::HomeScreenContent::HOME_SCREEN_MAIN_AND_AUX_TEMPERATURE;
    }
    case (1 << 7): {
      return Supla::HomeScreenContent::HOME_SCREEN_MODE_OR_TEMPERATURE;
    }
  }
}

uint64_t RemoteDeviceConfig::HomeScreenEnumToBit(enum HomeScreenContent type) {
  int number = static_cast<int>(type);
  return HomeScreenIntToBit(number);
}

uint64_t RemoteDeviceConfig::HomeScreenIntToBit(int mode) {
  if (mode < 0 || mode > 63) {
    mode = 0;
  }

  return (1ULL << mode);
}

void RemoteDeviceConfig::SetModbusProperties(
    const Supla::Modbus::ConfigProperties &properties) {
  modbusProperties = properties;
  RegisterConfigField(SUPLA_DEVICE_CONFIG_FIELD_MODBUS);
}

void RemoteDeviceConfig::processConfig(TSDS_SetDeviceConfig *config) {
  endFlagReceived = (config->EndOfDataFlag != 0);
  messageCounter++;
  if (resultCode != 255) {
    return;
  }

  if (isEndFlagReceived()) {
    resultCode = SUPLA_CONFIG_RESULT_TRUE;
    if (firstDeviceConfigAfterRegistration &&
        config->AvailableFields != fieldBitsUsedByDevice) {
      requireSetDeviceConfigFields =
          config->AvailableFields ^ fieldBitsUsedByDevice;
      SUPLA_LOG_INFO(
          "RemoteDeviceConfig: Config fields mismatch (0x%X%08X != 0x%X%08X) - "
          "sending device config for fields 0x%X%08X",
          PRINTF_UINT64_HEX(config->AvailableFields),
          PRINTF_UINT64_HEX(fieldBitsUsedByDevice),
          PRINTF_UINT64_HEX(requireSetDeviceConfigFields));
    }
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && cfg->isDeviceConfigChangeFlagSet()) {
    SUPLA_LOG_INFO(
        "RemoteDeviceConfig: local config change flag set, ignore set device "
        "config from server");
    if (cfg->isDeviceConfigChangeReadyToSend()) {
      requireSetDeviceConfigFields = fieldBitsUsedByDevice;
    }
    return;
  }

  // check size first
  uint32_t dataIndex = 0;
  uint64_t fieldBit = 1;
  while (dataIndex < config->ConfigSize && fieldBit) {
    if (fieldBit & config->Fields) {
      switch (fieldBit) {
        case SUPLA_DEVICE_CONFIG_FIELD_STATUS_LED: {
          dataIndex += sizeof(TDeviceConfig_StatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_POWER_STATUS_LED: {
          dataIndex += sizeof(TDeviceConfig_PowerStatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS: {
          dataIndex += sizeof(TDeviceConfig_ScreenBrightness);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_BUTTON_VOLUME: {
          dataIndex += sizeof(TDeviceConfig_ButtonVolume);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_DISABLE_USER_INTERFACE: {
          dataIndex += sizeof(TDeviceConfig_DisableUserInterface);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC: {
          dataIndex += sizeof(TDeviceConfig_AutomaticTimeSync);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY: {
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelay);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_CONTENT: {
          dataIndex += sizeof(TDeviceConfig_HomeScreenContent);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY_TYPE: {
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelayType);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_MODBUS: {
          dataIndex += sizeof(TDeviceConfig_Modbus);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_FIRMWARE_UPDATE: {
          dataIndex += sizeof(TDeviceConfig_FirmwareUpdate);
          break;
        }
        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%X%08X",
                            PRINTF_UINT64_HEX(fieldBit));
          resultCode = SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
          return;
        }
      }
    }
    fieldBit <<= 1;
  }
  if (dataIndex != config->ConfigSize) {
    SUPLA_LOG_WARNING(
        "RemoteDeviceConfig: precheck failed - invalid ConfigSize");
    resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
    return;
  }

  // actual parsing of DeviceConfig
  dataIndex = 0;
  fieldBit = 1;
  while (dataIndex < config->ConfigSize) {
    if (fieldBit & config->Fields) {
      switch (fieldBit) {
        case SUPLA_DEVICE_CONFIG_FIELD_STATUS_LED: {
          SUPLA_LOG_DEBUG("Processing StatusLed config");
          if (dataIndex + sizeof(TDeviceConfig_StatusLed) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processStatusLedConfig(fieldBit,
                                 reinterpret_cast<TDeviceConfig_StatusLed *>(
                                     config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_StatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_POWER_STATUS_LED: {
          SUPLA_LOG_DEBUG("Processing PowerStatusLed config");
          if (dataIndex + sizeof(TDeviceConfig_PowerStatusLed) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processPowerStatusLedConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_PowerStatusLed *>(config->Config +
                                                               dataIndex));
          dataIndex += sizeof(TDeviceConfig_PowerStatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS: {
          SUPLA_LOG_DEBUG("Processing ScreenBrightness config");
          if (dataIndex + sizeof(TDeviceConfig_ScreenBrightness) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processScreenBrightnessConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_ScreenBrightness *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreenBrightness);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_BUTTON_VOLUME: {
          SUPLA_LOG_DEBUG("Processing ButtonVolume config");
          if (dataIndex + sizeof(TDeviceConfig_ButtonVolume) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processButtonVolumeConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_ButtonVolume *>(config->Config +
                                                             dataIndex));
          dataIndex += sizeof(TDeviceConfig_ButtonVolume);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_DISABLE_USER_INTERFACE: {
          SUPLA_LOG_DEBUG("Processing DisableUserInterface config");
          if (dataIndex + sizeof(TDeviceConfig_DisableUserInterface) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processDisableUserInterfaceConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_DisableUserInterface *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_DisableUserInterface);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC: {
          SUPLA_LOG_DEBUG("Processing AutomaticTimeSync config");
          if (dataIndex + sizeof(TDeviceConfig_AutomaticTimeSync) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processAutomaticTimeSyncConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_AutomaticTimeSync *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_AutomaticTimeSync);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY: {
          SUPLA_LOG_DEBUG("Processing HomeScreenOffDelay config");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenOffDelay) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processHomeScreenDelayConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_HomeScreenOffDelay *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelay);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_CONTENT: {
          SUPLA_LOG_DEBUG("Processing HomeScreenContent config");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenContent) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processHomeScreenContentConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_HomeScreenContent *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenContent);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY_TYPE: {
          SUPLA_LOG_DEBUG("Processing HomeScreenOffDelayType config");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenOffDelayType) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processHomeScreenDelayTypeConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_HomeScreenOffDelayType *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelayType);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_MODBUS: {
          SUPLA_LOG_DEBUG("Processing Modbus config");
          if (dataIndex + sizeof(TDeviceConfig_Modbus) > config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processModbusConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_Modbus *>(config->Config +
                                                       dataIndex));
          dataIndex += sizeof(TDeviceConfig_Modbus);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_FIRMWARE_UPDATE: {
          SUPLA_LOG_DEBUG("Processing FirmwareUpdate config");
          if (dataIndex + sizeof(TDeviceConfig_FirmwareUpdate) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processFirmwareUpdateConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_FirmwareUpdate *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_FirmwareUpdate);
          break;
        }
        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%X%08X",
                            PRINTF_UINT64_HEX(fieldBit));
          resultCode = SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
          return;
        }
      }
    }
    fieldBit <<= 1;
    if (fieldBit == 0) {
      if (dataIndex != config->ConfigSize) {
        resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
        return;
      }
    }
  }
}

bool RemoteDeviceConfig::isEndFlagReceived() const {
  return endFlagReceived;
}

uint8_t RemoteDeviceConfig::getResultCode() const {
  return resultCode;
}

void RemoteDeviceConfig::processStatusLedConfig(
    uint64_t fieldBit, TDeviceConfig_StatusLed *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = -1;
    cfg->getInt8(Supla::ConfigTag::StatusLedCfgTag, &value);
    if (value >= 0 && value <= 2 && value != config->StatusLedType) {
      SUPLA_LOG_INFO("Setting StatusLedType to %d", config->StatusLedType);
      cfg->setInt8(Supla::ConfigTag::StatusLedCfgTag, config->StatusLedType);
      cfg->saveWithDelay(1000);

      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processPowerStatusLedConfig(
    uint64_t fieldBit, TDeviceConfig_PowerStatusLed *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = -1;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &value);
    if (value >= 0 && value <= 1 && value != config->Disabled) {
      SUPLA_LOG_INFO("Setting PowerStatusLed Disabled to %d",
                     config->Disabled);
      cfg->setInt8(Supla::ConfigTag::PowerStatusLedCfgTag,
                   config->Disabled);
      cfg->saveWithDelay(1000);

      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processScreenBrightnessConfig(uint64_t fieldBit,
    TDeviceConfig_ScreenBrightness *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool change = false;
    int32_t currentValue = -1;
    int32_t newValue = -1;
    cfg->getInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, &currentValue);
    if (currentValue < 0 || currentValue > 100) {
      currentValue = -1;  // automatic
    }
    if (config->Automatic == 1) {
      newValue = -1;  // automatic
    } else {
      newValue = config->ScreenBrightness;
    }
    if (newValue < -1 || newValue > 100) {
      newValue = -1;
    }
    if (newValue != currentValue) {
      SUPLA_LOG_INFO("Setting ScreenBrightness to %d", newValue);
      cfg->setInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, newValue);
      change = true;
    }

    int32_t currentAdjustmentForAutomaticValue = 0;
    int32_t newAdjustmentForAutomaticValue = 0;
    cfg->getInt32(
        Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
        &currentAdjustmentForAutomaticValue);

    newAdjustmentForAutomaticValue = config->AdjustmentForAutomatic;
    if (newAdjustmentForAutomaticValue > 100) {
      newAdjustmentForAutomaticValue = 100;
    }
    if (newAdjustmentForAutomaticValue < -100) {
      newAdjustmentForAutomaticValue = -100;
    }
    if (newAdjustmentForAutomaticValue != currentAdjustmentForAutomaticValue) {
      SUPLA_LOG_INFO("Setting AdjustmentForAutomatic to %d",
                     newAdjustmentForAutomaticValue);
      cfg->setInt32(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
                    newAdjustmentForAutomaticValue);
      change = true;
    }

    if (change) {
      cfg->saveWithDelay(1000);
      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processButtonVolumeConfig(uint64_t fieldBit,
    TDeviceConfig_ButtonVolume *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  uint8_t value = 100;
  cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &value);
  if (value > 100) {
    value = 100;
  }

  uint8_t newValue = config->Volume;
  if (newValue > 100) {
    newValue = 100;
  }
  if (newValue != value) {
    SUPLA_LOG_INFO("Setting Volume to %d", newValue);
    cfg->setUInt8(Supla::ConfigTag::VolumeCfgTag, newValue);
    cfg->saveWithDelay(1000);
    Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
  }
}

void RemoteDeviceConfig::processHomeScreenContentConfig(uint64_t fieldBit,
    TDeviceConfig_HomeScreenContent *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  SUPLA_LOG_DEBUG(
      "config->HomeScreenContent %X%08X, homeScreenContentAvailable %X%08X",
      PRINTF_UINT64_HEX(config->HomeScreenContent),
      PRINTF_UINT64_HEX(homeScreenContentAvailable));
  if ((config->HomeScreenContent & homeScreenContentAvailable) == 0) {
    SUPLA_LOG_WARNING(
        "Selected HomeScreenContent %X%08X is not supported by this device",
        PRINTF_UINT64_HEX(config->HomeScreenContent));
    return;
  }
  int8_t currentValue = 0;
  cfg->getInt8(Supla::ConfigTag::HomeScreenContentTag, &currentValue);
  if (currentValue < 0 || currentValue > 63) {
    currentValue = 0;
  }
  auto newValue =
      static_cast<int>(HomeScreenContentBitToEnum(config->HomeScreenContent));
  if (newValue != currentValue) {
    SUPLA_LOG_INFO("Setting HomeScreenContent to %d", newValue);
    cfg->setInt8(Supla::ConfigTag::HomeScreenContentTag, newValue);
    cfg->saveWithDelay(1000);
    Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
  }
}

void RemoteDeviceConfig::processHomeScreenDelayConfig(uint64_t fieldBit,
    TDeviceConfig_HomeScreenOffDelay *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  int32_t value = 0;
  cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &value);
  if (value < 0) {
    value = 0;
  }
  if (value > 65535) {
    value = 65535;
  }

  if (value != config->HomeScreenOffDelayS) {
    SUPLA_LOG_INFO("Setting HomeScreenOffDelay to %d",
                   config->HomeScreenOffDelayS);
    cfg->setInt32(Supla::ConfigTag::ScreenDelayCfgTag,
                  config->HomeScreenOffDelayS);
    cfg->saveWithDelay(1000);
    Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
  }
}

void RemoteDeviceConfig::processAutomaticTimeSyncConfig(uint64_t fieldBit,
    TDeviceConfig_AutomaticTimeSync *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 1;
    cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &value);
    if (value != config->AutomaticTimeSync && config->AutomaticTimeSync <= 1) {
      SUPLA_LOG_INFO("Setting AutomaticTimeSync to %d",
                     config->AutomaticTimeSync);
      cfg->setUInt8(Supla::AutomaticTimeSyncCfgTag, config->AutomaticTimeSync);
      cfg->saveWithDelay(1000);

      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processDisableUserInterfaceConfig(uint64_t fieldBit,
    TDeviceConfig_DisableUserInterface *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool change = false;
    uint8_t value = 0;
    int32_t minTempUI = 0;
    int32_t maxTempUI = 0;
    cfg->getUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, &value);
    cfg->getInt32(Supla::ConfigTag::MinTempUICfgTag, &minTempUI);
    cfg->getInt32(Supla::ConfigTag::MaxTempUICfgTag, &maxTempUI);
    if (value != config->DisableUserInterface &&
        config->DisableUserInterface <= 2) {
      SUPLA_LOG_INFO("Setting DisableUserInterface to %d",
                     config->DisableUserInterface);
      cfg->setUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag,
                    config->DisableUserInterface);
      change = true;
    }
    if (minTempUI != config->minAllowedTemperatureSetpointFromLocalUI) {
      SUPLA_LOG_INFO("Setting minAllowedTemperatureSetpointFromLocalUI to %d",
                     config->minAllowedTemperatureSetpointFromLocalUI);
      cfg->setInt32(Supla::ConfigTag::MinTempUICfgTag,
                    config->minAllowedTemperatureSetpointFromLocalUI);
      change = true;
    }
    if (maxTempUI != config->maxAllowedTemperatureSetpointFromLocalUI) {
      SUPLA_LOG_INFO("Setting maxAllowedTemperatureSetpointFromLocalUI to %d",
                     config->maxAllowedTemperatureSetpointFromLocalUI);
      cfg->setInt32(Supla::ConfigTag::MaxTempUICfgTag,
                    config->maxAllowedTemperatureSetpointFromLocalUI);
      change = true;
    }

    if (change) {
      cfg->saveWithDelay(1000);
      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::fillStatusLedConfig(
    TDeviceConfig_StatusLed *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = 0;
    cfg->getInt8(Supla::ConfigTag::StatusLedCfgTag, &value);
    if (value < 0 || value > 2) {
      value = 0;
    }
    SUPLA_LOG_DEBUG("Setting StatusLedType to %d (0x%X)", value, value);
    config->StatusLedType = value;
  }
}

void RemoteDeviceConfig::fillPowerStatusLedConfig(
    TDeviceConfig_PowerStatusLed *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int8_t value = 0;
    cfg->getInt8(Supla::ConfigTag::PowerStatusLedCfgTag, &value);
    if (value < 0 || value > 1) {
      value = 0;
    }
    SUPLA_LOG_DEBUG(
        "Setting PowerStatusLed Disabled to %d (0x%X)", value, value);
    config->Disabled = value;
  }
}


void RemoteDeviceConfig::fillScreenBrightnessConfig(
    TDeviceConfig_ScreenBrightness *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = -1;
    cfg->getInt32(Supla::ConfigTag::ScreenBrightnessCfgTag, &value);
    if (value == -1 || value < -1 || value > 100) {
      SUPLA_LOG_DEBUG("Setting ScreenBrightness automatic to 1 (0x01)");
      config->Automatic = 1;
    } else {
      SUPLA_LOG_DEBUG(
          "Setting ScreenBrightness to %d (0x%X)", value, value);
      config->ScreenBrightness = value;
    }

    int32_t adjustmentForAutomaticValue = 0;
    cfg->getInt32(Supla::ConfigTag::ScreenAdjustmentForAutomaticCfgTag,
                  &adjustmentForAutomaticValue);
    if (adjustmentForAutomaticValue > 100) {
      adjustmentForAutomaticValue = 100;
    }
    if (adjustmentForAutomaticValue < -100) {
      adjustmentForAutomaticValue = -100;
    }
    config->AdjustmentForAutomatic =
        static_cast<signed char>(adjustmentForAutomaticValue);
  }
}

void RemoteDeviceConfig::fillButtonVolumeConfig(
    TDeviceConfig_ButtonVolume *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 0;
    cfg->getUInt8(Supla::ConfigTag::VolumeCfgTag, &value);
    if (value > 100) {
      value = 100;
    }
    SUPLA_LOG_DEBUG("Setting Volume to %d (0x%02X)", value, value);
    config->Volume = value;
  }
}

void RemoteDeviceConfig::fillHomeScreenContentConfig(
    TDeviceConfig_HomeScreenContent *config) const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  int8_t value = 0;

  cfg->getInt8(Supla::ConfigTag::HomeScreenContentTag, &value);
  config->HomeScreenContent = HomeScreenIntToBit(value);
  SUPLA_LOG_DEBUG("Setting HomeScreenContent to %d (0x%02X)",
                  config->HomeScreenContent,
                  config->HomeScreenContent);
  SUPLA_LOG_DEBUG("Setting ModesAvailabe to 0x%04X",
                  homeScreenContentAvailable);
  config->ContentAvailable = homeScreenContentAvailable;
}

void RemoteDeviceConfig::fillHomeScreenDelayConfig(
    TDeviceConfig_HomeScreenOffDelay *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    cfg->getInt32(Supla::ConfigTag::ScreenDelayCfgTag, &value);
    if (value < 0) {
      value = 0;
    }
    if (value > 65535) {
      value = 65535;
    }
    uint16_t delayS = value;
    SUPLA_LOG_DEBUG(
        "Setting HomeScreenOffDelay to %d (0x%02X)", delayS, delayS);
    config->HomeScreenOffDelayS = delayS;
  }
}

void RemoteDeviceConfig::fillAutomaticTimeSyncConfig(
    TDeviceConfig_AutomaticTimeSync *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    uint8_t value = 1;  // by default it is enabled
    cfg->getUInt8(Supla::AutomaticTimeSyncCfgTag, &value);
    if (value > 1) {
      value = 1;
    }
    SUPLA_LOG_DEBUG("Setting AutomaticTimeSync to %d", value);
    config->AutomaticTimeSync = value;
  }
}

void RemoteDeviceConfig::fillDisableUserInterfaceConfig(
    TDeviceConfig_DisableUserInterface *config) const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  uint8_t value = 0;
  int32_t minTempUI = 0;
  int32_t maxTempUI = 0;
  cfg->getUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, &value);
  cfg->getInt32(Supla::ConfigTag::MinTempUICfgTag, &minTempUI);
  cfg->getInt32(Supla::ConfigTag::MaxTempUICfgTag, &maxTempUI);
  if (value > 2) {
    value = 2;
  }
  if (minTempUI < INT16_MIN) {
    minTempUI = INT16_MIN;
  }
  if (minTempUI > INT16_MAX) {
    minTempUI = INT16_MAX;
  }
  if (maxTempUI < INT16_MIN) {
    maxTempUI = INT16_MIN;
  }
  if (maxTempUI > INT16_MAX) {
    maxTempUI = INT16_MAX;
  }
  SUPLA_LOG_DEBUG("Setting minAllowedTemperatureSetpointFromLocalUI to %d",
                  minTempUI);
  config->minAllowedTemperatureSetpointFromLocalUI = minTempUI;
  SUPLA_LOG_DEBUG("Setting maxAllowedTemperatureSetpointFromLocalUI to %d",
                  maxTempUI);
  config->maxAllowedTemperatureSetpointFromLocalUI = maxTempUI;
  SUPLA_LOG_DEBUG("Setting DisableUserInterface to %d", value);
  config->DisableUserInterface = value;
}

bool RemoteDeviceConfig::isSetDeviceConfigRequired() const {
  return requireSetDeviceConfigFields != 0;
}

bool RemoteDeviceConfig::fillSetDeviceConfig(
    TSDS_SetDeviceConfig *config) const {
  if (config == nullptr) {
    return false;
  }

  // add sending in parts when config will be too big for single message
  // Currently all fields fits into single message, so we simplify the procedure
  config->EndOfDataFlag = 1;

  int dataIndex = 0;
  uint64_t remainingFileds = fieldBitsUsedByDevice;
  if (requireSetDeviceConfigFields != 0) {
    remainingFileds = requireSetDeviceConfigFields;
  }
  uint64_t fieldBit = 1;
  while (remainingFileds != 0) {
    if (fieldBit & remainingFileds) {
      // unset bit:
      remainingFileds &= ~fieldBit;
      config->Fields |= fieldBit;

      switch (fieldBit) {
        case SUPLA_DEVICE_CONFIG_FIELD_STATUS_LED: {
          SUPLA_LOG_DEBUG("Adding StatusLed config field");
          if (dataIndex + sizeof(TDeviceConfig_StatusLed) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillStatusLedConfig(reinterpret_cast<TDeviceConfig_StatusLed *>(
              config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_StatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_POWER_STATUS_LED: {
          SUPLA_LOG_DEBUG("Adding PowerStatusLed config field");
          if (dataIndex + sizeof(TDeviceConfig_PowerStatusLed) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillPowerStatusLedConfig(
              reinterpret_cast<TDeviceConfig_PowerStatusLed *>(config->Config +
                                                               dataIndex));
          dataIndex += sizeof(TDeviceConfig_PowerStatusLed);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_SCREEN_BRIGHTNESS: {
          SUPLA_LOG_DEBUG("Adding ScreenBrightness config field");
          if (dataIndex + sizeof(TDeviceConfig_ScreenBrightness) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillScreenBrightnessConfig(
              reinterpret_cast<TDeviceConfig_ScreenBrightness *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreenBrightness);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_BUTTON_VOLUME: {
          SUPLA_LOG_DEBUG("Adding ButtonVolume config field");
          if (dataIndex + sizeof(TDeviceConfig_ButtonVolume) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillButtonVolumeConfig(
              reinterpret_cast<TDeviceConfig_ButtonVolume *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ButtonVolume);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_DISABLE_USER_INTERFACE: {
          SUPLA_LOG_DEBUG("Adding DisableUserInterface config field");
          if (dataIndex + sizeof(TDeviceConfig_DisableUserInterface) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillDisableUserInterfaceConfig(
              reinterpret_cast<TDeviceConfig_DisableUserInterface *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_DisableUserInterface);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_AUTOMATIC_TIME_SYNC: {
          SUPLA_LOG_DEBUG("Adding AutomaticTimeSync config field");
          if (dataIndex + sizeof(TDeviceConfig_AutomaticTimeSync) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillAutomaticTimeSyncConfig(
              reinterpret_cast<TDeviceConfig_AutomaticTimeSync *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_AutomaticTimeSync);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY: {
          SUPLA_LOG_DEBUG("Adding HomeScreenOffDelay config field");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenOffDelay) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillHomeScreenDelayConfig(
              reinterpret_cast<TDeviceConfig_HomeScreenOffDelay *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelay);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_CONTENT: {
          SUPLA_LOG_DEBUG("Adding HomeScreenContent config field");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenContent) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillHomeScreenContentConfig(
              reinterpret_cast<TDeviceConfig_HomeScreenContent *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenContent);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY_TYPE: {
          SUPLA_LOG_DEBUG("Adding HomeScreenOffDelayType config field");
          if (dataIndex + sizeof(TDeviceConfig_HomeScreenOffDelayType) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillHomeScreenDelayTypeConfig(
              reinterpret_cast<TDeviceConfig_HomeScreenOffDelayType *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_HomeScreenOffDelayType);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_MODBUS: {
          SUPLA_LOG_DEBUG("Adding Modbus config field");
          if (dataIndex + sizeof(TDeviceConfig_Modbus) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillModbusConfig(reinterpret_cast<TDeviceConfig_Modbus *>(
              config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_Modbus);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_FIRMWARE_UPDATE: {
          SUPLA_LOG_DEBUG("Adding FirmwareUpdate config field");
          if (dataIndex + sizeof(TDeviceConfig_FirmwareUpdate) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillFirmwareUpdateConfig(
              reinterpret_cast<TDeviceConfig_FirmwareUpdate *>(config->Config +
                                                               dataIndex));
          dataIndex += sizeof(TDeviceConfig_FirmwareUpdate);
          break;
        }

        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%X%08X",
                            PRINTF_UINT64_HEX(fieldBit));
          break;
        }
      }
    }
    fieldBit <<= 1;
  }
  config->AvailableFields = fieldBitsUsedByDevice;
  config->ConfigSize = dataIndex;
  return true;
}

void RemoteDeviceConfig::handleSetDeviceConfigResult(
    TSDS_SetDeviceConfigResult *result) {
  (void)(result);
  if (result->Result == SUPLA_CONFIG_RESULT_TRUE) {
    ClearResendAttemptsCounter();
  }
  // received set device config result means that we updated all local
  // config changes to the server. We can clear localConfigChange flag
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (cfg->isDeviceConfigChangeReadyToSend()) {
      // we clear flag only if it is still set to "ready to send" state.
      // Other non 0 value means that there was config change done in meantime,
      // so we don't clear the flag and terminate current RemoteDeviceConfig
      // instance. Soon new instance will be started and will update config
      // one more time.
      cfg->clearDeviceConfigChangeFlag();
      cfg->saveWithDelay(1000);
    }
  }
}

void RemoteDeviceConfig::processHomeScreenDelayTypeConfig(uint64_t fieldBit,
    TDeviceConfig_HomeScreenOffDelayType *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (config == nullptr || cfg == nullptr) {
    return;
  }
  int32_t value = 0;
  cfg->getInt32(Supla::ConfigTag::ScreenDelayTypeCfgTag, &value);
  if (value > 1 || value < 0) {
    value = 0;
  }

  if (value != config->HomeScreenOffDelayType) {
    SUPLA_LOG_INFO("Setting HomeScreenOffDelayType to %d",
                   config->HomeScreenOffDelayType);
    cfg->setInt32(Supla::ConfigTag::ScreenDelayTypeCfgTag,
                  config->HomeScreenOffDelayType);
    cfg->saveWithDelay(1000);
    Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
  }
}

void RemoteDeviceConfig::fillHomeScreenDelayTypeConfig(
    TDeviceConfig_HomeScreenOffDelayType *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    cfg->getInt32(Supla::ConfigTag::ScreenDelayTypeCfgTag, &value);
    if (value > 1 || value < 0) {
      value = 0;
    }
    SUPLA_LOG_DEBUG(
        "Setting HomeScreenOffDelayType to %d", value);
    config->HomeScreenOffDelayType = value;
  }
}
void RemoteDeviceConfig::fillModbusConfig(TDeviceConfig_Modbus *config) const {
  if (config == nullptr) {
    return;
  }

  // copy properties
  config->Properties.Baudrate.B4800 = modbusProperties.baudrate.b4800;
  config->Properties.Baudrate.B9600 = modbusProperties.baudrate.b9600;
  config->Properties.Baudrate.B19200 = modbusProperties.baudrate.b19200;
  config->Properties.Baudrate.B38400 = modbusProperties.baudrate.b38400;
  config->Properties.Baudrate.B57600 = modbusProperties.baudrate.b57600;
  config->Properties.Baudrate.B115200 = modbusProperties.baudrate.b115200;
  config->Properties.StopBits.One = modbusProperties.stopBits.one;
  config->Properties.StopBits.OneAndHalf = modbusProperties.stopBits.oneAndHalf;
  config->Properties.StopBits.Two = modbusProperties.stopBits.two;
  config->Properties.Protocol.Master = modbusProperties.protocol.master;
  config->Properties.Protocol.Slave = modbusProperties.protocol.slave;
  config->Properties.Protocol.Rtu = modbusProperties.protocol.rtu;
  config->Properties.Protocol.Ascii = modbusProperties.protocol.ascii;
  config->Properties.Protocol.Tcp = modbusProperties.protocol.tcp;
  config->Properties.Protocol.Udp = modbusProperties.protocol.udp;

  // copy config
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    Supla::Modbus::Config modbusConfig = {};
    cfg->getBlob(
        Supla::ConfigTag::ModbusCfgTag, reinterpret_cast<char *>(&modbusConfig),
        sizeof(modbusConfig));

    config->ModbusAddress = modbusConfig.modbusAddress;
    config->Role = static_cast<unsigned char>(modbusConfig.role);
    config->SlaveTimeoutMs = modbusConfig.slaveTimeoutMs;
    config->SerialConfig.Mode =
        static_cast<unsigned char>(modbusConfig.serial.mode);
    config->SerialConfig.Baudrate = modbusConfig.serial.baudrate;
    config->SerialConfig.StopBits =
        static_cast<unsigned char>(modbusConfig.serial.stopBits);
    config->NetworkConfig.Mode =
        static_cast<unsigned char>(modbusConfig.network.mode);
    config->NetworkConfig.Port = modbusConfig.network.port;
  }
}

void RemoteDeviceConfig::processModbusConfig(uint64_t fieldBit,
                                             TDeviceConfig_Modbus *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    bool changed = false;
    bool valid = true;
    Supla::Modbus::Config modbusConfig = {};
    cfg->getBlob(
        Supla::ConfigTag::ModbusCfgTag, reinterpret_cast<char *>(&modbusConfig),
        sizeof(modbusConfig));

    if (modbusConfig.modbusAddress != config->ModbusAddress) {
      modbusConfig.modbusAddress = config->ModbusAddress;
      changed = true;
    }

    if (config->Role > MODBUS_ROLE_SLAVE) {
      SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid modbus role %d",
                        config->Role);
      valid = false;
    } else if (static_cast<unsigned char>(modbusConfig.role) != config->Role) {
      modbusConfig.role =
          static_cast<Supla::Modbus::Role>(config->Role);
      changed = true;
    }

    if (modbusConfig.slaveTimeoutMs != config->SlaveTimeoutMs) {
      modbusConfig.slaveTimeoutMs = config->SlaveTimeoutMs;
      changed = true;
    }

    if (config->SerialConfig.Mode > MODBUS_SERIAL_MODE_ASCII) {
      SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid modbus serial mode %d",
                        config->SerialConfig.Mode);
      valid = false;
    } else if (static_cast<unsigned char>(modbusConfig.serial.mode) !=
               config->SerialConfig.Mode) {
      modbusConfig.serial.mode =
          static_cast<Supla::Modbus::ModeSerial>(config->SerialConfig.Mode);
      changed = true;
    }

    if (modbusConfig.serial.baudrate != config->SerialConfig.Baudrate) {
      modbusConfig.serial.baudrate = config->SerialConfig.Baudrate;
      changed = true;
    }

    if (config->SerialConfig.StopBits > MODBUS_SERIAL_STOP_BITS_TWO) {
      SUPLA_LOG_WARNING(
          "RemoteDeviceConfig: invalid modbus serial stop bits %d",
          config->SerialConfig.StopBits);
      valid = false;
    } else if (static_cast<unsigned char>(modbusConfig.serial.stopBits) !=
               config->SerialConfig.StopBits) {
      modbusConfig.serial.stopBits = static_cast<Supla::Modbus::SerialStopBits>(
          config->SerialConfig.StopBits);
      changed = true;
    }

    if (modbusConfig.validateAndFix(modbusProperties)) {
      SUPLA_LOG_WARNING("RemoteDeviceConfig: modbus validation problem");
      valid = false;
      changed = true;
    }

    if (modbusProperties != config->Properties) {
      valid = false;
    }

    if (changed) {
      cfg->setBlob(
          Supla::ConfigTag::ModbusCfgTag,
          reinterpret_cast<const char *>(&modbusConfig),
          sizeof(modbusConfig));
      cfg->saveWithDelay(1000);
      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }

    if (!valid) {
      resendAttempts++;
      if (resendAttempts > 3) {
        SUPLA_LOG_WARNING(
            "RemoteDeviceConfig: resending modbus config failed too many "
            "times");
      } else {
        requireSetDeviceConfigFields |= fieldBit;
      }
    }
  }
}

void RemoteDeviceConfig::fillFirmwareUpdateConfig(
    TDeviceConfig_FirmwareUpdate *config) const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    auto otaPolicy = cfg->getAutoUpdatePolicy();
    switch (otaPolicy) {
      case Supla::AutoUpdatePolicy::Disabled: {
        config->Policy = SUPLA_FIRMWARE_UPDATE_POLICY_DISABLED;
        break;
      }
      case Supla::AutoUpdatePolicy::SecurityOnly: {
        config->Policy = SUPLA_FIRMWARE_UPDATE_POLICY_SECURITY_ONLY;
        break;
      }
      case Supla::AutoUpdatePolicy::AllUpdates: {
        config->Policy = SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED;
        break;
      }
      case Supla::AutoUpdatePolicy::ForcedOff: {
        config->Policy = SUPLA_FIRMWARE_UPDATE_POLICY_FORCED_OFF;
        break;
      }
    }
    config->Policy = static_cast<unsigned char>(otaPolicy);
  }
}

void RemoteDeviceConfig::processFirmwareUpdateConfig(
    uint64_t fieldBit, TDeviceConfig_FirmwareUpdate *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  bool valid = true;
  if (config->Policy > SUPLA_FIRMWARE_UPDATE_POLICY_ALL_ENABLED) {
    SUPLA_LOG_WARNING(
        "RemoteDeviceConfig: invalid firmware update policy %d",
        config->Policy);
    valid = false;
  } else if (cfg) {
    auto otaPolicy = static_cast<Supla::AutoUpdatePolicy>(config->Policy);
    auto currentPolicy = cfg->getAutoUpdatePolicy();
    if (otaPolicy != currentPolicy) {
      if (currentPolicy == Supla::AutoUpdatePolicy::ForcedOff) {
        SUPLA_LOG_INFO(
            "Firmware update is forced off. Changing policy is not allowed");
        valid = false;
      } else {
        SUPLA_LOG_INFO("Firmware update policy changed to %d", otaPolicy);
        cfg->setAutoUpdatePolicy(otaPolicy);
        cfg->saveWithDelay(1000);
      }
    }
    Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
  }

  if (!valid) {
    resendAttempts++;
    if (resendAttempts > 3) {
      SUPLA_LOG_WARNING(
          "RemoteDeviceConfig: resending firmware update config failed too "
          "many times");
    } else {
      requireSetDeviceConfigFields |= fieldBit;
    }
  }
}

