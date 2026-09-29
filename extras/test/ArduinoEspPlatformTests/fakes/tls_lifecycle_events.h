// Copyright (C) AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_TLS_LIFECYCLE_EVENTS_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_TLS_LIFECYCLE_EVENTS_H_

#include <string>
#include <vector>

inline std::vector<std::string> &TlsLifecycleEvents() {
  static std::vector<std::string> events;
  return events;
}

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_TLS_LIFECYCLE_EVENTS_H_
