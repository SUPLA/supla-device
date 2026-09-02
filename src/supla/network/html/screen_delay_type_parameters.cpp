// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "screen_delay_type_parameters.h"

#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ScreenDelayTypeParameters;

ScreenDelayTypeParameters::ScreenDelayTypeParameters() {
  setTag(Supla::ConfigTag::ScreenDelayTypeCfgTag);

  setLabel("Automatic screen off type");

  registerValue("Always enabled", 0);
  registerValue("Enabled only when it's dark", 1);
}

void ScreenDelayTypeParameters::onProcessingEnd() {
  if (configChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY_TYPE);
    }
  }
}

#endif  // ARDUINO_ARCH_AVR
