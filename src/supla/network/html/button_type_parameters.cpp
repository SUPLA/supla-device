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

#include "button_type_parameters.h"

#include <supla/network/web_sender.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/tools.h>
#include <supla/storage/config_tags.h>

#include <stdio.h>

using Supla::Html::ButtonTypeParameters;

ButtonTypeParameters::ButtonTypeParameters(int id) {
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  Supla::Config::generateKey(key, id, Supla::ConfigTag::BtnTypeTag);
  setTag(key);

  char label[100] = {};
  snprintf(label, sizeof(label), "IN%d type", id);
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

void ButtonTypeParameters::addDefualtOptions() {
  addMonostableOption();
  addBistableOption();
  addMotionSensorOption();
}

