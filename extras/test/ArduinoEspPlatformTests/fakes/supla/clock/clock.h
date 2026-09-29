// Copyright (C) AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_CLOCK_CLOCK_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_CLOCK_CLOCK_H_

namespace Supla {
class Clock {
 public:
  static bool IsReady() { return true; }
};
}  // namespace Supla

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_CLOCK_CLOCK_H_
