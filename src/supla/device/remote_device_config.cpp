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
#include <supla/device/status_led.h>
#include <supla/log_wrapper.h>
#include <supla/network/html/screen_brightness_parameters.h>
#include <supla/clock/clock.h>
#include <supla/network/html/volume_parameters.h>
#include <supla/network/html/screen_delay_parameters.h>
#include <supla/network/html/home_screen_content.h>
#include <supla/network/html/disable_user_interface_parameter.h>

using Supla::Device::RemoteDeviceConfig;

uint64_t RemoteDeviceConfig::fieldBitsUsedByDevice = 0;
uint64_t RemoteDeviceConfig::homeScreenContentAvailable = 0;

RemoteDeviceConfig::RemoteDeviceConfig(bool firstDeviceConfigAfterRegistration)
    : firstDeviceConfigAfterRegistration(firstDeviceConfigAfterRegistration) {
}

RemoteDeviceConfig::~RemoteDeviceConfig() {
}

void RemoteDeviceConfig::RegisterConfigField(uint64_t fieldBit) {
  if (fieldBit == 0 || (fieldBit & (fieldBit - 1)) != 0) {
    // (fieldBit & (fieldBit) - 1) will evaluate to 0 only when fieldBit had
    // only one bit set to 1 (that number was power of 2)
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid field 0x%08llx", fieldBit);
    return;
  }
  if (!(fieldBitsUsedByDevice & fieldBit)) {
    SUPLA_LOG_INFO("RemoteDeviceConfig: Registering field 0x%08llx", fieldBit);
  }
  fieldBitsUsedByDevice |= fieldBit;
}

void RemoteDeviceConfig::SetHomeScreenContentAvailable(uint64_t allValues) {
  SUPLA_LOG_INFO("RemoteDeviceConfig: SetHomeScreenContentAvailable 0x%08llx",
                 allValues);
  homeScreenContentAvailable = allValues;
  SUPLA_LOG_INFO("RemoteDeviceConfig: SetHomeScreenContentAvailable 0x%08llx",
                 homeScreenContentAvailable);
}

enum Supla::HomeScreenContent RemoteDeviceConfig::HomeScreenContentBitToEnum(
    uint64_t fieldBit) {
  if (fieldBit == 0 || (fieldBit & (fieldBit - 1)) != 0) {
    // (fieldBit & (fieldBit) - 1) will evaluate to 0 only when fieldBit had
    // only one bit set to 1 (that number was power of 2)
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid field 0x%08llx", fieldBit);
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
      requireSetDeviceConfig = true;
      SUPLA_LOG_INFO(
          "RemoteDeviceConfig: Config fields mismatch (0x%08llx != 0x%08llx) - "
          "sending full device config",
          config->AvailableFields,
          fieldBitsUsedByDevice);
    }
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg && cfg->isDeviceConfigChangeFlagSet()) {
    SUPLA_LOG_INFO(
        "RemoteDeviceConfig: local config change flag set, ignore set device "
        "config from server");
    if (cfg->isDeviceConfigChangeReadyToSend()) {
      requireSetDeviceConfig = true;
    }
    return;
  }

  // check size first
  int dataIndex = 0;
  uint64_t fieldBit = 1;
  while (dataIndex < config->ConfigSize && fieldBit) {
    if (fieldBit & config->Fields) {
      switch (fieldBit) {
        case SUPLA_DEVICE_CONFIG_FIELD_STATUS_LED: {
          dataIndex += sizeof(TDeviceConfig_StatusLed);
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
        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%08llx",
                            fieldBit);
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
        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%08llx",
                            fieldBit);
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
    cfg->getInt8(Supla::Device::StatusLedCfgTag, &value);
    if (value >= 0 && value <= 2 && value != config->StatusLedType) {
      SUPLA_LOG_INFO("Setting StatusLedType to %d", config->StatusLedType);
      cfg->setInt8(Supla::Device::StatusLedCfgTag, config->StatusLedType);
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
    cfg->getInt32(Supla::Html::ScreenBrightnessCfgTag, &currentValue);
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
      cfg->setInt32(Supla::Html::ScreenBrightnessCfgTag, newValue);
      change = true;
    }

    int32_t currentAdjustmentForAutomaticValue = 0;
    int32_t newAdjustmentForAutomaticValue = 0;
    cfg->getInt32(
        Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
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
      cfg->setInt32(Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
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
  cfg->getUInt8(Supla::Html::VolumeCfgTag, &value);
  if (value > 100) {
    value = 100;
  }

  uint8_t newValue = config->Volume;
  if (newValue > 100) {
    newValue = 100;
  }
  if (newValue != value) {
    SUPLA_LOG_INFO("Setting Volume to %d", newValue);
    cfg->setUInt8(Supla::Html::VolumeCfgTag, newValue);
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
      "config->HomeScreenContent %04x, homeScreenContentAvailable %08llx",
      config->HomeScreenContent,
      homeScreenContentAvailable);
  if ((config->HomeScreenContent & homeScreenContentAvailable) == 0) {
    SUPLA_LOG_WARNING(
        "Selected HomeScreenContent %d is not supported by this device",
        config->HomeScreenContent);
    return;
  }
  int8_t currentValue = 0;
  cfg->getInt8(Supla::HomeScreenContentTag, &currentValue);
  if (currentValue < 0 || currentValue > 63) {
    currentValue = 0;
  }
  auto newValue =
      static_cast<int>(HomeScreenContentBitToEnum(config->HomeScreenContent));
  if (newValue != currentValue) {
    SUPLA_LOG_INFO("Setting HomeScreenContent to %d", newValue);
    cfg->setInt8(Supla::HomeScreenContentTag, newValue);
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
  cfg->getInt32(Supla::Html::ScreenDelayCfgTag, &value);
  if (value < 0) {
    value = 0;
  }
  if (value > 65535) {
    value = 65535;
  }

  if (value != config->HomeScreenOffDelayS) {
    SUPLA_LOG_INFO("Setting HomeScreenOffDelay to %d",
                   config->HomeScreenOffDelayS);
    cfg->setInt32(Supla::Html::ScreenDelayCfgTag, config->HomeScreenOffDelayS);
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
    cfg->getUInt8(Supla::Html::DisableUserInterfaceCfgTag, &value);
    cfg->getInt32(Supla::Html::MinTempUICfgTag, &minTempUI);
    cfg->getInt32(Supla::Html::MaxTempUICfgTag, &maxTempUI);
    if (value != config->DisableUserInterface &&
        config->DisableUserInterface <= 2) {
      SUPLA_LOG_INFO("Setting DisableUserInterface to %d",
                     config->DisableUserInterface);
      cfg->setUInt8(Supla::Html::DisableUserInterfaceCfgTag,
                    config->DisableUserInterface);
      change = true;
    }
    if (minTempUI != config->minAllowedTemperatureSetpointFromLocalUI) {
      SUPLA_LOG_INFO("Setting minAllowedTemperatureSetpointFromLocalUI to %d",
                     config->minAllowedTemperatureSetpointFromLocalUI);
      cfg->setInt32(Supla::Html::MinTempUICfgTag,
                    config->minAllowedTemperatureSetpointFromLocalUI);
      change = true;
    }
    if (maxTempUI != config->maxAllowedTemperatureSetpointFromLocalUI) {
      SUPLA_LOG_INFO("Setting maxAllowedTemperatureSetpointFromLocalUI to %d",
                     config->maxAllowedTemperatureSetpointFromLocalUI);
      cfg->setInt32(Supla::Html::MaxTempUICfgTag,
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
    cfg->getInt8(Supla::Device::StatusLedCfgTag, &value);
    if (value < 0 || value > 2) {
      value = 0;
    }
    SUPLA_LOG_DEBUG("Setting StatusLedType to %d (0x%X)", value, value);
    config->StatusLedType = value;
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
    cfg->getInt32(Supla::Html::ScreenBrightnessCfgTag, &value);
    if (value == -1 || value < -1 || value > 100) {
      SUPLA_LOG_DEBUG("Setting ScreenBrightness automatic to 1 (0x01)");
      config->Automatic = 1;
    } else {
      SUPLA_LOG_DEBUG(
          "Setting ScreenBrightness to %d (0x%X)", value, value);
      config->ScreenBrightness = value;
    }

    int32_t adjustmentForAutomaticValue = 0;
    cfg->getInt32(Supla::Html::ScreenAdjustmentForAutomaticCfgTag,
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
    cfg->getUInt8(Supla::Html::VolumeCfgTag, &value);
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

  cfg->getInt8(Supla::HomeScreenContentTag, &value);
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
    cfg->getInt32(Supla::Html::ScreenDelayCfgTag, &value);
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
  cfg->getUInt8(Supla::Html::DisableUserInterfaceCfgTag, &value);
  cfg->getInt32(Supla::Html::MinTempUICfgTag, &minTempUI);
  cfg->getInt32(Supla::Html::MaxTempUICfgTag, &maxTempUI);
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
  return requireSetDeviceConfig;
}

bool RemoteDeviceConfig::fillFullSetDeviceConfig(
    TSDS_SetDeviceConfig *config) const {
  if (config == nullptr) {
    return false;
  }

  // add sending in parts when config will be too big for single message
  // Currently all fields fits into single message, so we simplify the procedure
  config->EndOfDataFlag = 1;

  int dataIndex = 0;
  uint64_t remainingFileds = fieldBitsUsedByDevice;
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
        default: {
          SUPLA_LOG_WARNING("RemoteDeviceConfig: unknown field 0x%08llx",
                            fieldBit);
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

