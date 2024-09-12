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

#include "rgbw_button_parameters.h"

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

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

