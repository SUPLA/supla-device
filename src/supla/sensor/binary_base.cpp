// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "binary_base.h"

#include <supla/time.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/storage/config_tags.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/element_with_channel_actions.h>
#include <string.h>

using Supla::Sensor::BinaryBase;


BinaryBase::BinaryBase() {
  channel.setType(SUPLA_CHANNELTYPE_BINARYSENSOR);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
}

BinaryBase::~BinaryBase() {
}

void BinaryBase::beginInitialChannelValueRead() {
  localActionState = LocalActionState::Initializing;
  initialStateCandidatePending = false;
}

void BinaryBase::setChannelValueQuietly(bool value) {
  char channelValue[SUPLA_CHANNELVALUE_SIZE] = {};
  channelValue[0] = value;
  channel.setNewValue(channelValue);
}

void BinaryBase::setInitialChannelValue(bool value) {
  if (localActionState != LocalActionState::Initializing) {
    beginInitialChannelValueRead();
  }
  setChannelValueQuietly(value);
  startupSyncStartTimeMs = millis();
  localActionState = turnActionSyncOnStartup ? LocalActionState::StartupSync
                                             : LocalActionState::Runtime;
}

void BinaryBase::notifyInputStateChangeCandidate() {
  if (localActionState == LocalActionState::Initializing) {
    initialStateCandidatePending = true;
  } else if (localActionState == LocalActionState::StartupSync) {
    initialStateCandidatePending = false;
    startupSyncStartTimeMs = millis();
  }
}

void BinaryBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  localActionState = LocalActionState::LoadingConfig;
  initialStateCandidatePending = false;
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();
    loadConfigChangeFlag();

    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::BinarySensorServerInvertedLogicTag);
    uint8_t storedServerInvertLogic = 0;
    cfg->getUInt8(key, &storedServerInvertLogic);
    setServerInvertLogic(storedServerInvertLogic > 0, false);

    if (config.filteringTimeMs > 0 || config.timeoutDs > 0 ||
        config.sensitivity > 0 || config.alarmMuted > 0) {
      generateKey(key, Supla::ConfigTag::BinarySensorCfgTag);
      BinarySensorConfig storedConfig = {};
      cfg->getBlob(key,
                   reinterpret_cast<char *>(&storedConfig),
                   sizeof(BinarySensorConfig));

      if (config.filteringTimeMs > 0) {
        if (storedConfig.filteringTimeMs > 0) {
          setFilteringTimeMs(storedConfig.filteringTimeMs, false);
        }
      }

      if (config.timeoutDs > 0) {
        if (storedConfig.timeoutDs > 0) {
          setTimeoutDs(storedConfig.timeoutDs, false);
        }
      }

      if (config.sensitivity > 0) {
        if (storedConfig.sensitivity > 0) {
          setSensitivity(storedConfig.sensitivity, false);
        }
      }

      if (config.alarmMuted > 0) {
        if (storedConfig.alarmMuted > 0) {
          setAlarmMuted(storedConfig.alarmMuted, false);
        }
      }
    }

    printConfig();
  }
}

void BinaryBase::printConfig() {
  SUPLA_LOG_INFO(
      "Binary[%d] config serverInvertLogic %d, timeoutDs %d, filteringTimeMs "
      "%d, sensitivity %d, alarmMuted %d",
      getChannelNumber(),
      channel.isServerInvertLogic(),
      config.timeoutDs,
      config.filteringTimeMs,
      config.sensitivity,
      config.alarmMuted);
}

void BinaryBase::purgeConfig() {
  Supla::ElementWithChannelActions::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::BinarySensorServerInvertedLogicTag);
  cfg->eraseKey(key);
  generateKey(key, Supla::ConfigTag::BinarySensorCfgTag);
  cfg->eraseKey(key);
}

Supla::ApplyConfigResult BinaryBase::applyChannelConfig(
    TSD_ChannelConfig *newConfig, bool local) {
  SUPLA_LOG_DEBUG("Binary[%d] processing%s channel config",
                  getChannelNumber(),
                  local ? " local" : "");

  if (newConfig->ConfigSize == 0) {
    SUPLA_LOG_DEBUG("Binary[%d] config missing on server",
        getChannelNumber());
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }

  if (newConfig->ConfigSize < sizeof(TChannelConfig_BinarySensor)) {
    return Supla::ApplyConfigResult::DataError;
  }

  TChannelConfig_BinarySensor *serverConfig =
      reinterpret_cast<TChannelConfig_BinarySensor *>(newConfig->Config);

  SUPLA_LOG_DEBUG("Binary[%d] received serverInvertLogic %d, timeoutDs %d, "
                  "filteringTimeMs %d, sensitivity %d, alarmMuted %d",
                  getChannelNumber(),
                  serverConfig->InvertedLogic,
                  serverConfig->Timeout,
                  serverConfig->FilteringTimeMs,
                  serverConfig->Sensitivity,
                  serverConfig->AlarmMuted);

  bool configChanged = false;

  if (setServerInvertLogic(serverConfig->InvertedLogic > 0, false)) {
    configChanged = true;
  }
  auto result = Supla::ApplyConfigResult::Success;
  if (getTimeoutDs() > 0 && serverConfig->Timeout == 0) {
    result = Supla::ApplyConfigResult::SetChannelConfigNeeded;
  } else {
    if (setTimeoutDs(serverConfig->Timeout, false)) {
      configChanged = true;
    }
  }
  if (getFilteringTimeMs() > 0 &&
      serverConfig->FilteringTimeMs == 0) {
    result = Supla::ApplyConfigResult::SetChannelConfigNeeded;
  } else {
    if (setFilteringTimeMs(serverConfig->FilteringTimeMs, false)) {
      configChanged = true;
    }
  }
  if (getSensitivity() > 0 && serverConfig->Sensitivity == 0) {
    result = Supla::ApplyConfigResult::SetChannelConfigNeeded;
  } else {
    if (setSensitivity(serverConfig->Sensitivity, false)) {
      configChanged = true;
    }
  }
  if (getAlarmMuted() > 0 && serverConfig->AlarmMuted == 0) {
    result = Supla::ApplyConfigResult::SetChannelConfigNeeded;
  } else {
    if (setAlarmMuted(serverConfig->AlarmMuted, false)) {
      configChanged = true;
    }
  }

  if (configChanged) {
    saveConfig();
  }
  printConfig();
  return result;
}

void BinaryBase::iterateAlways() {
  const uint32_t readTime = millis();
  if (readTime - lastReadTime > readIntervalMs) {
    lastReadTime = readTime;
    const bool previousLogicalValue = channel.getValueBool();
    const bool newRawValue = getValue();
    const bool newLogicalValue =
        channel.isServerInvertLogic() ? !newRawValue : newRawValue;

    if (localActionState == LocalActionState::StartupSync &&
        initialStateCandidatePending &&
        previousLogicalValue != newLogicalValue) {
      setChannelValueQuietly(newRawValue);
      runAction(newLogicalValue ? Supla::ON_TURN_ON : Supla::ON_TURN_OFF);
      initialStateCandidatePending = false;
      localActionState = LocalActionState::Runtime;
      return;
    }

    channel.setNewValue(newRawValue);

    if (previousLogicalValue != channel.getValueBool()) {
      // A real input transition already ran the complete set of channel
      // actions. It also makes a later startup synchronization unnecessary.
      initialStateCandidatePending = false;
      localActionState = LocalActionState::Runtime;
      return;
    }

    const uint32_t now = millis();
    if (localActionState == LocalActionState::StartupSync &&
        now - startupSyncStartTimeMs >= config.filteringTimeMs) {
      runAction(channel.getValueBool() ? Supla::ON_TURN_ON
                                       : Supla::ON_TURN_OFF);
      initialStateCandidatePending = false;
      localActionState = LocalActionState::Runtime;
    }
  }
}

Supla::Channel *BinaryBase::getChannel() {
  return &channel;
}

const Supla::Channel *BinaryBase::getChannel() const {
  return &channel;
}

bool BinaryBase::setServerInvertLogic(bool invertLogic, bool local) {
  if (invertLogic == channel.isServerInvertLogic()) {
    return false;
  }
  const bool previousLogicalValue = channel.getValueBool();
  channel.setServerInvertLogic(invertLogic);

  if (previousLogicalValue != channel.getValueBool() &&
      localActionState != LocalActionState::LoadingConfig &&
      localActionState != LocalActionState::Initializing) {
    runAction(channel.getValueBool() ? Supla::ON_TURN_ON
                                     : Supla::ON_TURN_OFF);
    runAction(Supla::ON_CHANGE);
    runAction(Supla::ON_SECONDARY_CHANNEL_CHANGE);
    initialStateCandidatePending = false;
    localActionState = LocalActionState::Runtime;
  }

  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    saveConfig();
  }
  return true;
}

void BinaryBase::setTurnActionSyncOnStartup(bool enabled) {
  turnActionSyncOnStartup = enabled;
  if (!enabled) {
    if (localActionState == LocalActionState::StartupSync) {
      initialStateCandidatePending = false;
      localActionState = LocalActionState::Runtime;
    }
    return;
  }

  if (localActionState == LocalActionState::Runtime) {
    startupSyncStartTimeMs = millis();
    localActionState = LocalActionState::StartupSync;
  }
}

bool BinaryBase::setSensitivity(uint8_t sensitivity, bool local) {
  if (sensitivity == config.sensitivity ||
      (config.sensitivity > 0 && sensitivity == 0) || sensitivity > 101) {
    return false;
  }
  config.sensitivity = sensitivity;
  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    saveConfig();
  }
  return true;
}

uint8_t BinaryBase::getSensitivity() const {
  return config.sensitivity;
}

bool BinaryBase::setAlarmMuted(uint8_t alarmMuted, bool local) {
  if (alarmMuted == config.alarmMuted ||
      (config.alarmMuted > 0 && alarmMuted == 0) || alarmMuted > 2) {
    return false;
  }
  config.alarmMuted = alarmMuted;
  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    saveConfig();
  }
  return true;
}

uint8_t BinaryBase::getAlarmMuted() const {
  return config.alarmMuted;
}

bool BinaryBase::setFilteringTimeMs(uint16_t filteringTimeMs, bool local) {
  if (filteringTimeMs == config.filteringTimeMs ||
      (config.filteringTimeMs > 0 && filteringTimeMs == 0)) {
    return false;
  }
  config.filteringTimeMs = filteringTimeMs;
  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    saveConfig();
  }
  return true;
}

uint16_t BinaryBase::getFilteringTimeMs() const {
  return config.filteringTimeMs;
}

bool BinaryBase::setTimeoutDs(uint16_t timeoutDs, bool local) {
  if (timeoutDs == config.timeoutDs ||
      (config.timeoutDs > 0 && timeoutDs == 0)) {
    return false;
  }
  config.timeoutDs = timeoutDs;
  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT);
    saveConfig();
  }
  return true;
}

uint16_t BinaryBase::getTimeoutDs() const {
  return config.timeoutDs;
}

bool BinaryBase::isServerInvertLogic() const {
  return channel.isServerInvertLogic();
}

void BinaryBase::setReadIntervalMs(uint32_t intervalMs) {
  if (intervalMs == 0) {
    intervalMs = 100;
  }
  readIntervalMs = intervalMs;
}

void BinaryBase::fillChannelConfig(void *channelConfig,
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

  auto serverConfig = reinterpret_cast<TChannelConfig_BinarySensor *>(
          channelConfig);
  *size = sizeof(TChannelConfig_BinarySensor);
  memset(serverConfig, 0, sizeof(TChannelConfig_BinarySensor));
  serverConfig->InvertedLogic = channel.isServerInvertLogic() ? 1 : 0;
  if (config.sensitivity > 0) {
    serverConfig->Sensitivity = config.sensitivity;
  }
  if (config.alarmMuted > 0) {
    serverConfig->AlarmMuted = config.alarmMuted;
  }
  if (config.timeoutDs > 0) {
    serverConfig->Timeout = config.timeoutDs;
  }
  if (config.filteringTimeMs > 0) {
    serverConfig->FilteringTimeMs = config.filteringTimeMs;
  }
}

void BinaryBase::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::BinarySensorServerInvertedLogicTag);
    if (!cfg->setUInt8(key, channel.isServerInvertLogic() ? 1 : 0)) {
      SUPLA_LOG_WARNING("Binary[%d] failed to save serverInvertLogic",
                        getChannelNumber());
    }

    generateKey(key, Supla::ConfigTag::BinarySensorCfgTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<const char *>(&config),
                     sizeof(BinarySensorConfig))) {
      SUPLA_LOG_INFO("Binary[%d] config saved successfully",
                     getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("Binary[%d] failed to save config",
                        getChannelNumber());
    }

    saveConfigChangeFlag();
    cfg->saveWithDelay(5000);
  }
  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}
