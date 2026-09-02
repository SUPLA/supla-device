// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <thread>  // NOLINT(build/c++11)

#include "linux_timers.h"

void supla10msTimer() {
  while (1) {
    SuplaDevice.onTimer();
    delay(10);
  }
}

void supla1msTimer() {
  while (1) {
    SuplaDevice.onFastTimer();
    delay(1);
  }
}

void Supla::Linux::Timers::init() {
  SUPLA_LOG_DEBUG("Starting linux timers...");
  std::thread standardTimer(supla10msTimer);
  standardTimer.detach();

  std::thread fastTimer(supla1msTimer);
  fastTimer.detach();
}
