// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "virtual_valve.h"

using Supla::Control::VirtualValve;

VirtualValve::VirtualValve(bool openClose) : ValveBase(openClose) {}

void VirtualValve::setValueOnDevice(uint8_t openLevel) {
  valveOpenState = openLevel;
}

uint8_t VirtualValve::getValueOpenStateFromDevice() {
  return valveOpenState;
}

