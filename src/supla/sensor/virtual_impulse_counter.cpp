// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "virtual_impulse_counter.h"

#include <string.h>
#include <supla/actions.h>
#include <supla/events.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/mqtt/impulse_counter_mqtt.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

#ifndef ARDUINO_ARCH_AVR
namespace Supla {
namespace Protocol {

// Keep this definition beside the IC constructor. A weak reference in Mqtt
// binds to it only when an IC is linked from the SDK archive.
void RegisterImpulseCounterMqttIfLinked() {
  RegisterImpulseCounterMqttHandler();
}

}  // namespace Protocol
}  // namespace Supla
#endif

using Supla::Sensor::VirtualImpulseCounter;

namespace {

bool isValidRate(int32_t rate) {
  return rate >= 0 && rate <= VirtualImpulseCounter::MaxImpulsesPerUnit;
}

}  // namespace

VirtualImpulseCounter::VirtualImpulseCounter() {
  channel.setType(SUPLA_CHANNELTYPE_IMPULSE_COUNTER);
  channel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RESET_COUNTERS);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_IC_WATER_METER);
  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  channel.config.ImpulsesPerUnit = defaultImpulsesPerUnit;
}

Supla::Channel *VirtualImpulseCounter::getChannel() {
  return &channel;
}

const Supla::Channel *VirtualImpulseCounter::getChannel() const {
  return &channel;
}

void VirtualImpulseCounter::onInit() {
}

void VirtualImpulseCounter::onLoadConfig(SuplaDeviceClass *) {
  channel.config = {};
  hasCounterConfig = false;
  channel.config.ImpulsesPerUnit = defaultImpulsesPerUnit;
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    loadFunctionFromConfig();
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ImpulseCounterConfigTag);
    TChannelConfig_ImpulseCounter saved = {};
    if (cfg->getBlobSize(key) == sizeof(saved) &&
        cfg->getBlob(key, reinterpret_cast<char *>(&saved), sizeof(saved)) &&
        isValidRate(saved.ImpulsesPerUnit)) {
      saved.CustomUnit[sizeof(saved.CustomUnit) - 1] = '\0';
      channel.config = saved;
      hasCounterConfig = true;
    }
    loadConfigChangeFlag();
  }
}

void VirtualImpulseCounter::purgeConfig() {
  Supla::ElementWithChannelActions::purgeConfig();
  if (auto cfg = Supla::Storage::ConfigInstance()) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ImpulseCounterConfigTag);
    cfg->eraseKey(key);
  }
}

void VirtualImpulseCounter::onRegistered(Supla::Protocol::SuplaSrpc *srpc) {
  ignorePreviousDefaultReply = false;
  ElementWithChannelActions::onRegistered(srpc);
}

void VirtualImpulseCounter::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (ignorePreviousDefaultReply && result &&
      result->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT) {
    ignorePreviousDefaultReply = false;
    return;
  }
  ElementWithChannelActions::handleSetChannelConfigResult(result);
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
  if (counter != value) {
    SUPLA_LOG_DEBUG("VirtualImpulseCounter[%d] - set counter to %d",
                    channel.getChannelNumber(),
                    static_cast<int>(value));
  }
  counter = value;
  channel.setNewValue(value);
}

void VirtualImpulseCounter::incCounter() {
  counter++;
  runAction(Supla::ON_IMPULSE);
}

void VirtualImpulseCounter::iterateAlways() {
  if (__atomic_load_n(&counterResetRequested, __ATOMIC_RELAXED)) {
    // Requests arriving before the flag is cleared share the upcoming reset.
    __atomic_store_n(&counterResetRequested, false, __ATOMIC_RELAXED);
    resetCounter();
  }
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
      __atomic_store_n(&counterResetRequested, true, __ATOMIC_RELAXED);
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
  if (forceStateSaveOnChange) {
    Supla::Storage::WriteStateStorage();
  }
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

void VirtualImpulseCounter::setDefaultImpulsesPerUnit(
    uint32_t impulsesPerUnit) {
  if (impulsesPerUnit > 0 && impulsesPerUnit <= MaxImpulsesPerUnit) {
    if (!hasCounterConfig) {
      channel.config.ImpulsesPerUnit = impulsesPerUnit;
    }
    defaultImpulsesPerUnit = impulsesPerUnit;
  }
}

int32_t VirtualImpulseCounter::getImpulsesPerUnit() const {
  return channel.config.ImpulsesPerUnit;
}

bool VirtualImpulseCounter::setImpulsesPerUnit(int32_t impulsesPerUnit) {
  if (!isValidRate(impulsesPerUnit)) {
    return false;
  }
  auto config = channel.config;
  config.ImpulsesPerUnit = impulsesPerUnit;
  return storeConfig(config, true);
}

const char *VirtualImpulseCounter::getCustomUnit() const {
  return channel.config.CustomUnit;
}

bool VirtualImpulseCounter::setCustomUnit(const char *unit) {
  auto config = channel.config;
  if (!unit || strnlen(unit, sizeof(config.CustomUnit)) ==
                   sizeof(config.CustomUnit)) {
    return false;
  }
  if (strcmp(config.CustomUnit, unit) == 0) {
    return true;
  }
  const char *end = unit;
  while (*end) {
    if (static_cast<unsigned char>(*end) < 0x20) {
      return false;
    }
    end++;
  }
  memset(config.CustomUnit, 0, sizeof(config.CustomUnit));
  memcpy(config.CustomUnit, unit, end - unit);
  return storeConfig(config, true);
}

double VirtualImpulseCounter::getCalculatedValue(uint64_t count) const {
  return channel.calculate(count);
}

void VirtualImpulseCounter::setLocalFunction(uint32_t function) {
  if (!setAndSaveFunction(function)) {
    return;
  }
  for (auto protocol = Supla::Protocol::ProtocolLayer::first(); protocol;
       protocol = protocol->next()) {
    protocol->notifyConfigChange(getChannelNumber());
  }
}

bool VirtualImpulseCounter::setAndSaveFunction(uint32_t function) {
  if (!ElementWithChannelActions::setAndSaveFunction(function)) {
    return false;
  }

  const bool localConfigInFlight =
      getChannelConfigState() == Supla::ChannelConfigState::LocalChangeSent;
  clearLocalConfigChanges(SUPLA_CONFIG_TYPE_DEFAULT);
  if (localConfigInFlight && getNextLocalConfigType() < 0) {
    // An ACK for the previous function must not acknowledge its replacement.
    ignorePreviousDefaultReply = true;
    setChannelConfigState(Supla::ChannelConfigState::None);
    setChannelConfigAttempts = 0;
  }
  awaitingConfigForFunction = true;
  return true;
}

Supla::ApplyConfigResult VirtualImpulseCounter::applyChannelConfig(
    TSD_ChannelConfig *result, bool local) {
  if (result->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return Supla::ApplyConfigResult::NotSupported;
  }
  if (result->ConfigSize == 0) {
    if (awaitingConfigForFunction) {
      TChannelConfig_ImpulseCounter defaults = {};
      defaults.ImpulsesPerUnit = defaultImpulsesPerUnit;
      if (!storeConfig(defaults, false)) {
        return Supla::ApplyConfigResult::DataError;
      }
      awaitingConfigForFunction = false;
    }
    return Supla::ApplyConfigResult::SetChannelConfigNeeded;
  }
  if (result->ConfigSize != sizeof(channel.config)) {
    return Supla::ApplyConfigResult::DataError;
  }
  TChannelConfig_ImpulseCounter config = {};
  memcpy(&config, result->Config, sizeof(config));
  config.CustomUnit[sizeof(config.CustomUnit) - 1] = '\0';
  if (!isValidRate(config.ImpulsesPerUnit) || !storeConfig(config, local)) {
    return Supla::ApplyConfigResult::DataError;
  }
  if (!local) {
    awaitingConfigForFunction = false;
  }
  return Supla::ApplyConfigResult::Success;
}

void VirtualImpulseCounter::fillChannelConfig(
    void *config, int *size, uint8_t type) {
  if (config && size && type == SUPLA_CONFIG_TYPE_DEFAULT) {
    memcpy(config, &channel.config, sizeof(channel.config));
    *size = sizeof(channel.config);
  }
}

bool VirtualImpulseCounter::storeConfig(
    const TChannelConfig_ImpulseCounter &config, bool local) {
  if ((local || hasCounterConfig) &&
      memcmp(&channel.config, &config, sizeof(config)) == 0) {
    return true;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::ImpulseCounterConfigTag);
    if (!cfg->setBlob(
            key, reinterpret_cast<const char *>(&config), sizeof(config))) {
      return false;
    }
  }

  const bool valueConfigChanged =
      channel.config.ImpulsesPerUnit != config.ImpulsesPerUnit ||
      channel.config.InitialValue != config.InitialValue ||
      strcmp(channel.config.CustomUnit, config.CustomUnit) != 0;
  channel.config = config;
  hasCounterConfig = true;
  if (local) {
    triggerSetChannelConfig(SUPLA_CONFIG_TYPE_DEFAULT, true);
    saveConfigChangeFlag();
  }
  if (cfg) {
    cfg->saveWithDelay(5000);
  }
  if (valueConfigChanged) {
    for (auto protocol = Supla::Protocol::ProtocolLayer::first(); protocol;
         protocol = protocol->next()) {
      protocol->notifyConfigChange(getChannelNumber());
    }
  }
  return true;
}
