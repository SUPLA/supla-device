// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/timer.h>
#include <timer_mock.h>

TimerInterface *timerInterfaceInstance = nullptr;

TimerInterface::TimerInterface() {
  timerInterfaceInstance = this;
}

TimerInterface::~TimerInterface() {
  timerInterfaceInstance = nullptr;
}


void Supla::initTimers() {
  assert(timerInterfaceInstance);
  timerInterfaceInstance->initTimers();
}

TimerMock::TimerMock() {}
TimerMock::~TimerMock() {}
