// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "button_config_parameters.h"

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

#include <stdio.h>

using Supla::Html::ButtonConfigParameters;

ButtonConfigParameters::ButtonConfigParameters(int id) {
  if (id >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, id, Supla::ConfigTag::BtnConfigTag);
    setTag(key);
  } else {
    setTag(Supla::ConfigTag::BtnConfigTag);
  }

  char label[100] = {};
  if (id >= 0) {
    snprintf(label, sizeof(label), "IN%d Config", id);
  } else {
    snprintf(label, sizeof(label), "IN Config");
  }
  setLabel(label);

  registerValue("ON", 0);
  registerValue("OFF", 1);
}


#endif  // ARDUINO_ARCH_AVR
