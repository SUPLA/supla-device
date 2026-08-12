// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "memory_variable_driver.h"

using Supla::Sensor::MemoryVariableDriver;


void MemoryVariableDriver::initialize() {
}

double MemoryVariableDriver::getValue() {
  return value;
}

void MemoryVariableDriver::setValue(const double &value) {
  this->value = value;
}
