// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "button_type_parameters.h"

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

#include <stdio.h>

using Supla::Html::ButtonTypeParameters;

ButtonTypeParameters::ButtonTypeParameters(int id, const char *labelPrefix) {
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, id, Supla::ConfigTag::BtnTypeTag);
  setTag(key);

  char label[100] = {};
  if (labelPrefix) {
    snprintf(label, sizeof(label), "%s type", labelPrefix);
  } else {
    snprintf(label, sizeof(label), "IN%d type", id);
  }
  setLabel(label);
}

void ButtonTypeParameters::addMonostableOption() {
  registerValue("MONOSTABLE", 0);
}

void ButtonTypeParameters::addBistableOption() {
  registerValue("BISTABLE", 1);
}

void ButtonTypeParameters::addMotionSensorOption() {
  registerValue("MOTION SENSOR", 2);
}

void ButtonTypeParameters::addCentralControlOption() {
  registerValue("CENTRAL CONTROL", 3);
}

void ButtonTypeParameters::addDefaultOptions() {
  addMonostableOption();
  addBistableOption();
  addMotionSensorOption();
}


#endif  // ARDUINO_ARCH_AVR
