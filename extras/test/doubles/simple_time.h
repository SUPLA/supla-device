// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_
#define EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_

#include <stdint.h>
#include "arduino_mock.h"

class SimpleTime : public TimeInterface {
 public:
  uint32_t millis() override;
  void advance(int advanceMs);

  uint32_t value = 0;
};

#endif  // EXTRAS_TEST_DOUBLES_SIMPLE_TIME_H_
