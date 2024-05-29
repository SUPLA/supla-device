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

#include "channel_types.h"

#include <supla-common/proto.h>

using Supla::ChannelType;

int32_t Supla::channelTypeToProtoType(ChannelType type) {
  switch (type) {
    case ChannelType::BINARYSENSOR:
      return SUPLA_CHANNELTYPE_BINARYSENSOR;
    case ChannelType::DISTANCESENSOR:
      return SUPLA_CHANNELTYPE_DISTANCESENSOR;
    case ChannelType::RELAY:
      return SUPLA_CHANNELTYPE_RELAY;
    case ChannelType::THERMOMETER:
      return SUPLA_CHANNELTYPE_THERMOMETER;
    case ChannelType::HUMIDITYSENSOR:
      return SUPLA_CHANNELTYPE_HUMIDITYSENSOR;
    case ChannelType::HUMIDITYANDTEMPSENSOR:
      return SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR;
    case ChannelType::WINDSENSOR:
      return SUPLA_CHANNELTYPE_WINDSENSOR;
    case ChannelType::PRESSURESENSOR:
      return SUPLA_CHANNELTYPE_PRESSURESENSOR;
    case ChannelType::RAINSENSOR:
      return SUPLA_CHANNELTYPE_RAINSENSOR;
    case ChannelType::WEIGHTSENSOR:
      return SUPLA_CHANNELTYPE_WEIGHTSENSOR;
    case ChannelType::DIMMER:
      return SUPLA_CHANNELTYPE_DIMMER;
    case ChannelType::RGBLEDCONTROLLER:
      return SUPLA_CHANNELTYPE_RGBLEDCONTROLLER;
    case ChannelType::DIMMERANDRGBLED:
      return SUPLA_CHANNELTYPE_DIMMERANDRGBLED;
    case ChannelType::ELECTRICITY_METER:
      return SUPLA_CHANNELTYPE_ELECTRICITY_METER;
    case ChannelType::IMPULSE_COUNTER:
      return SUPLA_CHANNELTYPE_IMPULSE_COUNTER;
    case ChannelType::HVAC:
      return SUPLA_CHANNELTYPE_HVAC;
    case ChannelType::VALVE_OPENCLOSE:
      return SUPLA_CHANNELTYPE_VALVE_OPENCLOSE;
    case ChannelType::VALVE_PERCENTAGE:
      return SUPLA_CHANNELTYPE_VALVE_PERCENTAGE;
    case ChannelType::GENERAL_PURPOSE_MEASUREMENT:
      return SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT;
    case ChannelType::GENERAL_PURPOSE_METER:
      return SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER;
    case ChannelType::ACTIONTRIGGER:
      return SUPLA_CHANNELTYPE_ACTIONTRIGGER;
    case ChannelType::NOT_SET:
    default:
      return 0;
  }
}

ChannelType Supla::protoTypeToChannelType(int32_t type) {
  switch (type) {
    case SUPLA_CHANNELTYPE_BINARYSENSOR:
      return ChannelType::BINARYSENSOR;
    case SUPLA_CHANNELTYPE_DISTANCESENSOR:
      return ChannelType::DISTANCESENSOR;
    case SUPLA_CHANNELTYPE_RELAY:
      return ChannelType::RELAY;
    case SUPLA_CHANNELTYPE_THERMOMETER:
      return ChannelType::THERMOMETER;
    case SUPLA_CHANNELTYPE_HUMIDITYSENSOR:
      return ChannelType::HUMIDITYSENSOR;
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
      return ChannelType::HUMIDITYANDTEMPSENSOR;
    case SUPLA_CHANNELTYPE_WINDSENSOR:
      return ChannelType::WINDSENSOR;
    case SUPLA_CHANNELTYPE_PRESSURESENSOR:
      return ChannelType::PRESSURESENSOR;
    case SUPLA_CHANNELTYPE_RAINSENSOR:
      return ChannelType::RAINSENSOR;
    case SUPLA_CHANNELTYPE_WEIGHTSENSOR:
      return ChannelType::WEIGHTSENSOR;
    case SUPLA_CHANNELTYPE_DIMMER:
      return ChannelType::DIMMER;
    case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER:
      return ChannelType::RGBLEDCONTROLLER;
    case SUPLA_CHANNELTYPE_DIMMERANDRGBLED:
      return ChannelType::DIMMERANDRGBLED;
    case SUPLA_CHANNELTYPE_ELECTRICITY_METER:
      return ChannelType::ELECTRICITY_METER;
    case SUPLA_CHANNELTYPE_IMPULSE_COUNTER:
      return ChannelType::IMPULSE_COUNTER;
    case SUPLA_CHANNELTYPE_HVAC:
      return ChannelType::HVAC;
    case SUPLA_CHANNELTYPE_VALVE_OPENCLOSE:
      return ChannelType::VALVE_OPENCLOSE;
    case SUPLA_CHANNELTYPE_VALVE_PERCENTAGE:
      return ChannelType::VALVE_PERCENTAGE;
    case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT:
      return ChannelType::GENERAL_PURPOSE_MEASUREMENT;
    case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER:
      return ChannelType::GENERAL_PURPOSE_METER;
    case SUPLA_CHANNELTYPE_ACTIONTRIGGER:
      return ChannelType::ACTIONTRIGGER;
    default:
      return ChannelType::NOT_SET;
  }
}
