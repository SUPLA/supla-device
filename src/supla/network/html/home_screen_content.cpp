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

#include "home_screen_content.h"

#include <supla/storage/config.h>
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

