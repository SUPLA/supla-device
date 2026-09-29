// Copyright (C) AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_ARDUINO_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_ARDUINO_H_

#include <stdint.h>

#include <ctime>
#include <string>

using String = std::string;

struct rst_info {
  int reason = 0;
};

constexpr int REASON_SOFT_RESTART = 1;
constexpr int REASON_DEFAULT_RST = 2;

class EspTestStub {
 public:
  void restart() {}
  void random(uint8_t *, int) {}
  rst_info *getResetInfoPtr() { return &resetInfo; }

 private:
  rst_info resetInfo;
};

inline EspTestStub ESP;

inline void delay(uint32_t) {}
inline void configTime(int32_t, int32_t, const char *, const char *) {}

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_ARDUINO_H_
