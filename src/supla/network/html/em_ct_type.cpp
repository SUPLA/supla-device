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

#include "em_ct_type.h"

#include <supla/sensor/electricity_meter.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

using Supla::Html::EmCtTypeParameters;

EmCtTypeParameters::EmCtTypeParameters(Supla::Sensor::ElectricityMeter *em)
    : em(em) {
  if (em) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    Supla::Config::generateKey(
        key, em->getChannelNumber(), Supla::ConfigTag::EmCtTypeTag);
    setTag(key);
  }

  setLabel("Current transformer");

  if (em->isCtTypeSupported(EM_CT_TYPE_100A_33mA)) {
    registerValue("100A/33.3mA", 0);
  }
  if (em->isCtTypeSupported(EM_CT_TYPE_200A_66mA)) {
    registerValue("200A/66.6mA", 1);
  }
  if (em->isCtTypeSupported(EM_CT_TYPE_400A_133mA)) {
    registerValue("400A/133.3mA", 2);
  }
}

void EmCtTypeParameters::onProcessingEnd() {
  if (configChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setChannelConfigChangeFlag(em->getChannelNumber());
    }
    if (em) {
      em->onLoadConfig(nullptr);
    }
  }
  configChanged = false;
}
