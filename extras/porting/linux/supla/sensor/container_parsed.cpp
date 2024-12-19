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

#include "container_parsed.h"

#include <supla/log_wrapper.h>
#include "supla/sensor/container.h"

using Supla::Sensor::ContainerParsed;

ContainerParsed::ContainerParsed(Supla::Parser::Parser *parser)
    : SensorParsed(parser) {
}


void ContainerParsed::onInit() {
  channel.setNewValue(readNewValue());
}

int ContainerParsed::readNewValue() {
  int value = 0;

  if (isParameterConfigured(Supla::Parser::Level)) {
    if (refreshParserSource()) {
      value = getParameterValue(Supla::Parser::Level);
    }
    if (!parser->isValid()) {
      if (!isDataErrorLogged) {
        isDataErrorLogged = true;
        SUPLA_LOG_WARNING("ContainerParsed[%d]: source data error",
                          getChannelNumber());
      }
      return 0;
    }
    isDataErrorLogged = false;
  }
  return value;
}

