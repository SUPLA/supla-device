/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#include "condition.h"
#include "events.h"
#include "element.h"

Supla::Condition::Condition(double threshold, bool useAlternativeValue)
  : threshold(threshold),
  useAlternativeValue(useAlternativeValue) {
  }

Supla::Condition::Condition(double threshold, Supla::ConditionGetter *getter)
  : threshold(threshold), getter(getter) {
  }

Supla::Condition::~Condition() {
  if (getter) {
    delete getter;
    getter = nullptr;
  }
}

void Supla::Condition::handleAction(int event, int action) {
  if (event == Supla::ON_CHANGE ||
      event == Supla::ON_SECONDARY_CHANNEL_CHANGE) {
    if (!source->getChannel()) {
      return;
    }

    int channelType = source->getChannel()->getChannelType();

    // Read channel value
    double value = 0;
    bool isValid = true;

    if (getter) {
      value = getter->getValue(source, &isValid);
    } else {
      switch (channelType) {
        case SUPLA_CHANNELTYPE_DISTANCESENSOR:
        case SUPLA_CHANNELTYPE_THERMOMETER:
        case SUPLA_CHANNELTYPE_WINDSENSOR:
        case SUPLA_CHANNELTYPE_PRESSURESENSOR:
        case SUPLA_CHANNELTYPE_RAINSENSOR:
        case SUPLA_CHANNELTYPE_WEIGHTSENSOR:
          value = source->getChannel()->getValueDouble();
          break;
        case SUPLA_CHANNELTYPE_IMPULSE_COUNTER:
          value = source->getChannel()->getValueInt64();
          break;
        case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
        case SUPLA_CHANNELTYPE_HUMIDITYSENSOR:
          value = useAlternativeValue
            ? source->getChannel()->getValueDoubleSecond()
            : source->getChannel()->getValueDoubleFirst();
          break;
        case SUPLA_CHANNELTYPE_DIMMER:
          value = source->getChannel()->getValueBrightness();
          break;
        case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER:
          value = source->getChannel()->getValueColorBrightness();
          break;
        case SUPLA_CHANNELTYPE_DIMMERANDRGBLED:
          value = useAlternativeValue
            ? source->getChannel()->getValueColorBrightness()
            : source->getChannel()->getValueBrightness();
          break;
          /* case SUPLA_CHANNELTYPE_ELECTRICITY_METER: */

        default:
          return;
      }

      // Check channel value validity
      switch (channelType) {
        case SUPLA_CHANNELTYPE_DISTANCESENSOR:
        case SUPLA_CHANNELTYPE_WINDSENSOR:
        case SUPLA_CHANNELTYPE_PRESSURESENSOR:
        case SUPLA_CHANNELTYPE_RAINSENSOR:
        case SUPLA_CHANNELTYPE_WEIGHTSENSOR:
          isValid = value >= 0;
          break;
        case SUPLA_CHANNELTYPE_THERMOMETER:
          isValid = value >= -273;
          break;
        case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
        case SUPLA_CHANNELTYPE_HUMIDITYSENSOR:
          isValid = useAlternativeValue ? value >= 0 : value >= -273;
          break;
      }
    }
    if (checkConditionFor(value, isValid)) {
      client->handleAction(event, action);
    }
  }
}

// Condition objects will be deleted during ActionHandlerClient list cleanup
bool Supla::Condition::deleteClient() {
  return true;
}

bool Supla::Condition::checkConditionFor(double val, bool isValid) {
  if (!alreadyFired && condition(val, isValid)) {
    alreadyFired = true;
    return true;
  }
  if (alreadyFired) {
    if (!condition(val, isValid)) {
      alreadyFired = false;
    }
  }
  return false;
}

void Supla::Condition::setSource(Supla::Element *src) {
  source = src;
}

void Supla::Condition::setClient(Supla::ActionHandler *clientPtr) {
  client = clientPtr;
}

void Supla::Condition::setSource(Supla::Element &src) {
  setSource(&src);
}

void Supla::Condition::setClient(Supla::ActionHandler &clientPtr) {
  setClient(&clientPtr);
}

void Supla::Condition::activateAction(int action) {
  if (client) {
    client->activateAction(action);
  }
}
