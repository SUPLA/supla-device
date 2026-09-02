// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simple_time.h"

uint32_t SimpleTime::millis() {
  return value;
}

void SimpleTime::advance(int advanceMs) {
  value += advanceMs;
}
