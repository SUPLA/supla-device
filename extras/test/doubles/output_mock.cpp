// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "output_mock.h"

OutputSimulator::OutputSimulator() {
}

OutputSimulator::~OutputSimulator() {
}

int OutputSimulator::getOutputValue() const {
  return outputValue;
}

void OutputSimulator::setOutputValue(int value) {
  outputValue = value;
  setOutputValueCheck(value);
}

bool OutputSimulator::isOnOffOnly() const {
  return onOffOnly;
}

void OutputSimulator::setOutputValueCheck(int) {
}
