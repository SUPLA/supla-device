// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/events.h>
#include <supla/time.h>

#include "therm_hygro_press_meter.h"

Supla::Sensor::ThermHygroPressMeter::ThermHygroPressMeter() {
  pressureChannel.setType(SUPLA_CHANNELTYPE_PRESSURESENSOR);
  pressureChannel.setDefaultFunction(SUPLA_CHANNELFNC_PRESSURESENSOR);
}

Supla::Sensor::ThermHygroPressMeter::~ThermHygroPressMeter() {}

double Supla::Sensor::ThermHygroPressMeter::getPressure() {
  return PRESSURE_NOT_AVAILABLE;
}

void Supla::Sensor::ThermHygroPressMeter::iterateAlways() {
  if (millis() - lastReadTime > refreshIntervalMs) {
    pressureChannel.setNewValue(getPressure());
  }
  ThermHygroMeter::iterateAlways();
}

bool Supla::Sensor::ThermHygroPressMeter::iterateConnected() {
  bool response = true;
  if (pressureChannel.isUpdateReady()) {
    pressureChannel.sendUpdate();
    response = false;
  }

  if (!ThermHygroMeter::iterateConnected()) {
    response = false;
  }
  return response;
}

Supla::Element &Supla::Sensor::ThermHygroPressMeter::disableChannelState() {
  pressureChannel.unsetFlag(SUPLA_CHANNEL_FLAG_CHANNELSTATE);
  return ThermHygroMeter::disableChannelState();
}

Supla::Channel *Supla::Sensor::ThermHygroPressMeter::getSecondaryChannel() {
  return &pressureChannel;
}

const Supla::Channel *Supla::Sensor::ThermHygroPressMeter::getSecondaryChannel()
    const {
  return &pressureChannel;
}

void Supla::Sensor::ThermHygroPressMeter::addAction(uint16_t action,
                                            ActionHandler &client,
                                            uint16_t event,
                                            bool alwaysEnabled) {
  // delegate secondary channel event registration to secondary channel
  switch (event) {
    case Supla::ON_SECONDARY_CHANNEL_CHANGE: {
      getSecondaryChannel()->addAction(action, client, event, alwaysEnabled);
      return;
    }
  }
  // delegate all other events to primary channel
  channel.addAction(action, client, event, alwaysEnabled);
}

void Supla::Sensor::ThermHygroPressMeter::addAction(uint16_t action,
                                            ActionHandler *client,
                                            uint16_t event,
                                            bool alwaysEnabled) {
  ThermHygroPressMeter::addAction(action, *client, event, alwaysEnabled);
}
