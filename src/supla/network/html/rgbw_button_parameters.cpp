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

#include <string.h>
#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>

#include <stdio.h>

using Supla::Html::RgbwButtonParameters;

RgbwButtonParameters::RgbwButtonParameters(int id) {
  if (id >= 0) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(key, id, RgbwButtonTag);
    setTag(key);
  } else {
    setTag(RgbwButtonTag);
  }

  char label[100] = {};
  if (id >= 0) {
    snprintf(label, sizeof(label), "#%d RGBW outputs controlled by IN", id);
  } else {
    snprintf(label, sizeof(label), "RGBW outputs controlled by IN");
  }
  setLabel(label);

  registerValue("RGB+W", 0);
  registerValue("RGB", 1);
  registerValue("W", 2);
}

