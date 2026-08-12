// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "em_ct_type.h"

#include <supla/sensor/electricity_meter.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/storage/config_tags.h>

using Supla::Html::EmCtTypeParameters;

EmCtTypeParameters::EmCtTypeParameters(Supla::Sensor::ElectricityMeter *em)
    : em(em) {
  if (em) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key, em->getChannelNumber(), Supla::ConfigTag::EmCtTypeTag);
    setTag(key);
  }

  setLabel("Current transformer");

  if (em->isCtTypeSupported(EM_CT_TYPE_100A_33mA)) {
    registerValue("100A/33.3mA", 0);
  }
  if (em->isCtTypeSupported(EM_CT_TYPE_200A_66mA)) {
    registerValue("200A/66.6mA", 1);
  }
  if (em->isCtTypeSupported(EM_CT_TYPE_400A_133mA)) {
    registerValue("400A/133.3mA", 2);
  }
}

void EmCtTypeParameters::onProcessingEnd() {
  if (configChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelConfigChangeFlag(em->getChannelNumber());
    }
    if (em) {
      em->onLoadConfig(nullptr);
    }
  }
  configChanged = false;
}

#endif  // ARDUINO_ARCH_AVR
