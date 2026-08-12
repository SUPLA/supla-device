// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_TIMER_MOCK_H_
#define EXTRAS_TEST_DOUBLES_TIMER_MOCK_H_

#include <supla/timer.h>
#include <gmock/gmock.h>

class TimerInterface {
 public:
  TimerInterface();
  virtual ~TimerInterface();
  virtual void initTimers() = 0;
};

class TimerMock : public TimerInterface {
 public:
  TimerMock();
  virtual ~TimerMock();
  MOCK_METHOD((void), initTimers, (), (override));
};

#endif  // EXTRAS_TEST_DOUBLES_TIMER_MOCK_H_
