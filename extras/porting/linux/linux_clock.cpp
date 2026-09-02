// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_clock.h"
#include <supla/log_wrapper.h>

using Supla::LinuxClock;

LinuxClock::LinuxClock() {
  isClockReady = true;
  automaticTimeSync = false;

  SUPLA_LOG_INFO(
        "Clock: local time: %d-%d-%d %d:%d:%d",
        getYear(),
        getMonth(),
        getDay(),
        getHour(),
        getMin(),
        getSec());
}

void LinuxClock::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
}

void LinuxClock::onTimer() {
}
