// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <atomic>
#include <cstdlib>
#include <thread>  // NOLINT(build/c++11)

#include "linux_timers.h"

namespace {

std::atomic<bool> running{false};
std::thread standardTimer;
std::thread fastTimer;

void supla10msTimer() {
  while (running.load(std::memory_order_relaxed)) {
    SuplaDevice.onTimer();
    delay(10);
  }
}

void supla1msTimer() {
  while (running.load(std::memory_order_relaxed)) {
    SuplaDevice.onFastTimer();
    delay(1);
  }
}

}  // namespace

void Supla::Linux::Timers::init() {
  if (running.exchange(true, std::memory_order_relaxed)) {
    return;
  }
  SUPLA_LOG_DEBUG("Starting linux timers...");
  // Also stop on startup failures before SuplaDevice's mutex is destroyed.
  std::atexit(stop);
  standardTimer = std::thread(supla10msTimer);
  fastTimer = std::thread(supla1msTimer);
}

void Supla::Linux::Timers::stop() {
  running.store(false, std::memory_order_relaxed);
  if (standardTimer.joinable()) {
    standardTimer.join();
  }
  if (fastTimer.joinable()) {
    fastTimer.join();
  }
}
