// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "home_screen_content.h"

#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::HomeScreenContentParameters;

HomeScreenContentParameters::HomeScreenContentParameters(const char *label) {
  setTag(Supla::ConfigTag::HomeScreenContentTag);
  setLabel(label);
  setBaseTypeBitCount(8);
}

void HomeScreenContentParameters::initFields(uint64_t fieldBits) {
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_NONE) {
    registerValue("None", 0);
  }
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TEMPERATURE) {
    registerValue("Temperature", 1);
  }
  if (fieldBits &
             SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TEMPERATURE_AND_HUMIDITY) {
    registerValue("Temperature and humidity", 2);
  }
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TIME) {
    registerValue("Time", 3);
  }
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TIME_DATE) {
    registerValue("Time and date", 4);
  }
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_TEMPERATURE_TIME) {
    registerValue("Temperature and time", 5);
  }
  if (fieldBits &
             SUPLA_DEVCFG_HOME_SCREEN_CONTENT_MAIN_AND_AUX_TEMPERATURE) {
    registerValue("Main and auxiliary temperature", 6);
  }
  if (fieldBits & SUPLA_DEVCFG_HOME_SCREEN_CONTENT_MODE_OR_TEMPERATURE) {
    registerValue("Mode or temperature", 7);
  }
}

void HomeScreenContentParameters::onProcessingEnd() {
  if (configChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_CONTENT);
    }
  }
}


#endif  // ARDUINO_ARCH_AVR
