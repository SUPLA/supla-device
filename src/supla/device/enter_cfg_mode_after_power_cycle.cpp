// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "enter_cfg_mode_after_power_cycle.h"
#include <SuplaDevice.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/storage/config_tags.h>
#include <supla/tools.h>

using Supla::Device::EnterCfgModeAfterPowerCycle;

namespace Supla::Device {
const char PowerCycleKey[] = "power_cycle";
}  // namespace Supla::Device

EnterCfgModeAfterPowerCycle::EnterCfgModeAfterPowerCycle(uint32_t timeoutMs,
                                                         uint32_t powerCycles,
                                                         bool alwaysEnabled)
    : timeoutMs(timeoutMs),
      maxPowerCycles(powerCycles),
      alwaysEnabled(alwaysEnabled) {
  enabled = alwaysEnabled;
}

void EnterCfgModeAfterPowerCycle::onLoadConfig(SuplaDeviceClass *sdc) {
  this->sdc = sdc;
  timestampMs = millis();
  if (timestampMs == 0) {
    timestampMs = 1;
  }
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    if (!alwaysEnabled) {
      uint8_t disableUI = 0;
      cfg->getUInt8(Supla::ConfigTag::DisableUserInterfaceCfgTag, &disableUI);
      enabled = (disableUI == 1);
      if (cfg->getDeviceMode() == Supla::DeviceMode::DEVICE_MODE_TEST) {
        enabled = true;
      }
    }

    cfg->getUInt32(Supla::Device::PowerCycleKey, &currentPowerCycle);
    SUPLA_LOG_DEBUG("PowerCycle: %d (%s)", currentPowerCycle,
                    enabled ? "enabled" : "disabled");
  }
}

void EnterCfgModeAfterPowerCycle::iterateAlways() {
  if (!enabled || timestampMs == 0) {
    return;
  }

  if (incrementOnlyOnPowerResetReason) {
    if (!Supla::isLastResetPower()) {
      // this will skip incrementing
      incremented = true;
    }
  }

  if (!incremented) {
    currentPowerCycle++;

    if (currentPowerCycle > maxPowerCycles) {
      sdc->enterConfigMode();
    } else {
      auto cfg = Supla::Storage::ConfigInstance();
      if (cfg) {
        cfg->setUInt32(Supla::Device::PowerCycleKey, currentPowerCycle);
        cfg->commit();
      }
    }
    incremented = true;
  }

  if (millis() - timestampMs > timeoutMs) {
    currentPowerCycle = 0;
    timestampMs = 0;
    SUPLA_LOG_DEBUG("PowerCycle: timeout, counter reset to 0");
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setUInt32(Supla::Device::PowerCycleKey, currentPowerCycle);
      cfg->saveWithDelay(1000);
    }
  }
}

void EnterCfgModeAfterPowerCycle::setAlwaysEnabled(bool alwaysEnabled) {
  this->alwaysEnabled = alwaysEnabled;
  enabled = alwaysEnabled;
}

void EnterCfgModeAfterPowerCycle::setIncrementOnlyOnPowerResetReason(
    bool value) {
  incrementOnlyOnPowerResetReason = value;
}

void EnterCfgModeAfterPowerCycle::resetCounter() {
  if (currentPowerCycle == 0 && timestampMs == 0) {
    return;
  }

  currentPowerCycle = 0;
  timestampMs = 0;
  incremented = true;
  SUPLA_LOG_DEBUG("PowerCycle: counter reset to 0");
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->setUInt32(Supla::Device::PowerCycleKey, currentPowerCycle);
    cfg->commit();
  }
}
