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

using Supla::Device::RemoteDeviceConfig;

uint64_t RemoteDeviceConfig::fieldBitsUsedByDevice = 0;

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

  int dataIndex = 0;
  uint64_t fieldBit = 1;
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
        case SUPLA_DEVICE_CONFIG_FIELD_DISABLE_LOCAL_CONFIG: {
          SUPLA_LOG_DEBUG("Processing DisableLocalConfig config");
          if (dataIndex + sizeof(TDeviceConfig_DisableLocalConfig) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processDisableLocalConfigConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_DisableLocalConfig *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_DisableLocalConfig);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_TIMEZONE_OFFSET: {
          SUPLA_LOG_DEBUG("Processing TimezoneOffset config");
          if (dataIndex + sizeof(TDeviceConfig_TimezoneOffset) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processTimezoneOffsetConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_TimezoneOffset *>(config->Config +
                                                               dataIndex));
          dataIndex += sizeof(TDeviceConfig_TimezoneOffset);
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
        case SUPLA_DEVICE_CONFIG_FIELD_SCREENSAVER_DELAY: {
          SUPLA_LOG_DEBUG("Processing ScreensaverDelay config");
          if (dataIndex + sizeof(TDeviceConfig_ScreensaverDelay) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processScreensaverDelayConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_ScreensaverDelay *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreensaverDelay);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_SCREENSAVER_MODE: {
          SUPLA_LOG_DEBUG("Processing ScreensaverMode config");
          if (dataIndex + sizeof(TDeviceConfig_ScreensaverMode) >
              config->ConfigSize) {
            SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid ConfigSize");
            resultCode = SUPLA_CONFIG_RESULT_DATA_ERROR;
            return;
          }
          processScreensaverModeConfig(
              fieldBit,
              reinterpret_cast<TDeviceConfig_ScreensaverMode *>(config->Config +
                                                                dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreensaverMode);
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
      cfg->saveWithDelay(1000);

      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processTimezoneOffsetConfig(uint64_t fieldBit,
    TDeviceConfig_TimezoneOffset *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t offset = 0;
    cfg->getInt32(Supla::TimezoneOffsetMinCfgTag, &offset);
    if (offset < -1560) {
      offset = -1560;
    }
    if (offset > 1560) {
      offset = 1560;
    }
    int32_t newValue = config->TimezoneOffsetMinutes;
    if (newValue < -1560) {
      newValue = -1560;
    }
    if (newValue > 1560) {
      newValue = 1560;
    }
    if (newValue != offset) {
      SUPLA_LOG_INFO("Setting TimezoneOffset to %d", newValue);
      cfg->setInt32(Supla::TimezoneOffsetMinCfgTag, newValue);
      cfg->saveWithDelay(1000);
      Supla::Element::NotifyElementsAboutConfigChange(fieldBit);
    }
  }
}

void RemoteDeviceConfig::processButtonVolumeConfig(uint64_t fieldBit,
    TDeviceConfig_ButtonVolume *config) {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
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
}

void RemoteDeviceConfig::processScreensaverModeConfig(uint64_t fieldBit,
    TDeviceConfig_ScreensaverMode *config) {
  (void)(fieldBit);
  (void)(config);
}

void RemoteDeviceConfig::processScreensaverDelayConfig(uint64_t fieldBit,
    TDeviceConfig_ScreensaverDelay *config) {
  (void)(fieldBit);
  (void)(config);
}

void RemoteDeviceConfig::processAutomaticTimeSyncConfig(uint64_t fieldBit,
    TDeviceConfig_AutomaticTimeSync *config) {
  (void)(fieldBit);
  (void)(config);
}

void RemoteDeviceConfig::processDisableLocalConfigConfig(uint64_t fieldBit,
    TDeviceConfig_DisableLocalConfig *config) {
  (void)(fieldBit);
  (void)(config);
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
  }
}

void RemoteDeviceConfig::fillTimezoneOffsetConfig(
    TDeviceConfig_TimezoneOffset *config) const {
  if (config == nullptr) {
    return;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    int32_t value = 0;
    cfg->getInt32(Supla::TimezoneOffsetMinCfgTag, &value);
    if (value < -1560) {
      value = -1560;
    }
    if (value > 1560) {
      value = 1560;
    }
    SUPLA_LOG_DEBUG("Setting TimezoneOffset to %d (0x%04X)", value, value);
    config->TimezoneOffsetMinutes = value;
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

void RemoteDeviceConfig::fillScreensaverModeConfig(
    TDeviceConfig_ScreensaverMode *config) const {
  (void)(config);
}

void RemoteDeviceConfig::fillScreensaverDelayConfig(
    TDeviceConfig_ScreensaverDelay *config) const {
  (void)(config);
}

void RemoteDeviceConfig::fillAutomaticTimeSyncConfig(
    TDeviceConfig_AutomaticTimeSync *config) const {
  (void)(config);
}

void RemoteDeviceConfig::fillDisableLocalConfigConfig(
    TDeviceConfig_DisableLocalConfig *config) const {
  (void)(config);
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
        case SUPLA_DEVICE_CONFIG_FIELD_DISABLE_LOCAL_CONFIG: {
          SUPLA_LOG_DEBUG("Adding DisableLocalConfig config field");
          if (dataIndex + sizeof(TDeviceConfig_DisableLocalConfig) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillDisableLocalConfigConfig(
              reinterpret_cast<TDeviceConfig_DisableLocalConfig *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_DisableLocalConfig);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_TIMEZONE_OFFSET: {
          SUPLA_LOG_DEBUG("Adding TimezoneOffset config field");
          if (dataIndex + sizeof(TDeviceConfig_TimezoneOffset) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillTimezoneOffsetConfig(
              reinterpret_cast<TDeviceConfig_TimezoneOffset *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_TimezoneOffset);
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
        case SUPLA_DEVICE_CONFIG_FIELD_SCREENSAVER_DELAY: {
          SUPLA_LOG_DEBUG("Adding ScreensaverDelay config field");
          if (dataIndex + sizeof(TDeviceConfig_ScreensaverDelay) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillScreensaverDelayConfig(
              reinterpret_cast<TDeviceConfig_ScreensaverDelay *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreensaverDelay);
          break;
        }
        case SUPLA_DEVICE_CONFIG_FIELD_SCREENSAVER_MODE: {
          SUPLA_LOG_DEBUG("Adding ScreensaverMode config field");
          if (dataIndex + sizeof(TDeviceConfig_ScreensaverMode) >
              SUPLA_DEVICE_CONFIG_MAXSIZE) {
            SUPLA_LOG_ERROR("RemoteDeviceConfig: ConfigSize too big");
            return false;
          }
          fillScreensaverModeConfig(
              reinterpret_cast<TDeviceConfig_ScreensaverMode *>(
                  config->Config + dataIndex));
          dataIndex += sizeof(TDeviceConfig_ScreensaverMode);
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

