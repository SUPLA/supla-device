// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "condition.h"

#include <math.h>
#include <supla/element_with_channel_actions.h>
#include <supla/channels/channel.h>

#include "events.h"

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
      event == Supla::ON_SECONDARY_CHANNEL_CHANGE ||
      event == Supla::ON_COUNTDOWN_TIMER) {
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
        case SUPLA_CHANNELTYPE_WEIGHTSENSOR: {
          value = source->getChannel()->getValueDouble();
          break;
        }
        case SUPLA_CHANNELTYPE_IMPULSE_COUNTER: {
          value = source->getChannel()->getValueInt64();
          break;
        }
        case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
        case SUPLA_CHANNELTYPE_HUMIDITYSENSOR: {
          value = useAlternativeValue
            ? source->getChannel()->getValueDoubleSecond()
            : source->getChannel()->getValueDoubleFirst();
          break;
        }
        case SUPLA_CHANNELTYPE_DIMMER: {
          value = source->getChannel()->getValueBrightness();
          break;
        }
        case SUPLA_CHANNELTYPE_RGBLEDCONTROLLER: {
          value = source->getChannel()->getValueColorBrightness();
          break;
        }
        case SUPLA_CHANNELTYPE_DIMMERANDRGBLED: {
          value = useAlternativeValue
            ? source->getChannel()->getValueColorBrightness()
            : source->getChannel()->getValueBrightness();
          break;
        }
        case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER:
        case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT: {
          value = source->getChannel()->getValueDouble();
          break;
        }
        case SUPLA_CHANNELTYPE_CONTAINER: {
          value = source->getChannel()->getContainerFillValue();
          break;
        }
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
        case SUPLA_CHANNELTYPE_WEIGHTSENSOR: {
          isValid = value >= 0;
          break;
        }
        case SUPLA_CHANNELTYPE_THERMOMETER: {
          isValid = value >= -273;
          break;
        }
        case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
        case SUPLA_CHANNELTYPE_HUMIDITYSENSOR: {
          isValid = useAlternativeValue ? value >= 0 : value >= -273;
          break;
        }
        case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_METER:
        case SUPLA_CHANNELTYPE_GENERAL_PURPOSE_MEASUREMENT: {
          isValid = isnan(value) ? false : true;
          break;
        }
        case SUPLA_CHANNELTYPE_CONTAINER: {
          isValid = value >= 0 && value <= 100;
          break;
        }
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

void Supla::Condition::setSource(Supla::ElementWithChannelActions *src) {
  source = src;
}

void Supla::Condition::setClient(Supla::ActionHandler *clientPtr) {
  client = clientPtr;
}

void Supla::Condition::setSource(Supla::ElementWithChannelActions &src) {
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

void Supla::Condition::setThreshold(double val) {
  threshold = val;
  if (source) {
    source->runAction(Supla::ON_CHANGE);
  }
}

Supla::ActionHandler *Supla::Condition::getRealClient() {
  if (client) {
    return client;
  }
  return this;
}
