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

#include "screen_delay_type_parameters.h"

#include <supla/storage/config.h>
#include <supla/element.h>
#include <supla/storage/config_tags.h>

using Supla::Html::ScreenDelayTypeParameters;

ScreenDelayTypeParameters::ScreenDelayTypeParameters() {
  setTag(Supla::ConfigTag::ScreenDelayTypeCfgTag);

  setLabel("Automatic screen off type");

  registerValue("Always enabled", 0);
  registerValue("Enabled only when it's dark", 1);
}

void ScreenDelayTypeParameters::onProcessingEnd() {
  if (configChanged) {
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->setDeviceConfigChangeFlag();
      Supla::Element::NotifyElementsAboutConfigChange(
          SUPLA_DEVICE_CONFIG_FIELD_HOME_SCREEN_OFF_DELAY_TYPE);
    }
  }
}
