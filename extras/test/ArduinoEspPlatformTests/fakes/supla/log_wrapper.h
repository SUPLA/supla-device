// Copyright (C) AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_LOG_WRAPPER_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_LOG_WRAPPER_H_

#include <stdio.h>

#define SUPLA_LOG_DEBUG(...) \
  do {                       \
    if (false) {             \
      fprintf(stderr, __VA_ARGS__); \
    }                        \
  } while (0)
#define SUPLA_LOG_ERROR(...) SUPLA_LOG_DEBUG(__VA_ARGS__)
#define SUPLA_LOG_INFO(...) SUPLA_LOG_DEBUG(__VA_ARGS__)
#define SUPLA_LOG_WARNING(...) SUPLA_LOG_DEBUG(__VA_ARGS__)

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_SUPLA_LOG_WRAPPER_H_
