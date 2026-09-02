// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "remote_output_interface.h"

using Supla::Control::RemoteOutputInterface;

RemoteOutputInterface::RemoteOutputInterface(bool onOffOnly) :
    onOffOnly(onOffOnly) {
}

RemoteOutputInterface::~RemoteOutputInterface() {
}

void RemoteOutputInterface::setOutputValueFromRemote(int value) {
  if (value > 100) {
    value = 100;
  }
  outputValue = value;
}

bool RemoteOutputInterface::isControlledInternally() const {
  return false;
}

int RemoteOutputInterface::getOutputValue() const {
  return outputValue;
}

void RemoteOutputInterface::setOutputValue(int) {
  // ignore value set by Hvac
  // This thermostat is controlled externally
}

bool RemoteOutputInterface::isOnOffOnly() const {
  return onOffOnly;
}

