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

