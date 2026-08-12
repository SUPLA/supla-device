// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ARDUINO_ARCH_AVR
#include "rgbw_button_parameters.h"

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>
#include <supla/channels/channel.h>

#include <stdio.h>

using Supla::Html::RgbwButtonParameters;

RgbwButtonParameters::RgbwButtonParameters(int id, const char *labelValue) {
  if (id >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, id, Supla::ConfigTag::RgbwButtonTag);
    setTag(key);
  } else {
    setTag(Supla::ConfigTag::RgbwButtonTag);
  }

  auto el = Supla::Element::getElementByChannelNumber(id);
  int channelType = 0;
  if (el) {
    channelType = el->getChannel()->getChannelType();
  }

  char label[100] = {};
  if (labelValue != nullptr) {
    setLabel(labelValue);
  } else {
    if (id >= 0) {
      snprintf(label, sizeof(label), "#%d %s output controlled by IN", id,
          channelType == SUPLA_CHANNELTYPE_DIMMER ? "Dimmer" :
          (channelType == SUPLA_CHANNELTYPE_RGBLEDCONTROLLER ? "RGB" :
           "RGBW"));
    } else {
      snprintf(label, sizeof(label), "RGBW output controlled by IN");
    }
    setLabel(label);
  }

  switch (channelType) {
    case SUPLA_CHANNELTYPE_DIMMER: {
      registerValue("YES", 2);
      registerValue("NO", 3);
      break;
    }
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
      registerValue("YES", 1);
      registerValue("NO", 3);
      break;
    }
    default: {
      registerValue("RGB+W", 0);
      registerValue("RGB", 1);
      registerValue("W", 2);
      registerValue("NONE", 3);
      break;
    }
  }
}


#endif  // ARDUINO_ARCH_AVR
