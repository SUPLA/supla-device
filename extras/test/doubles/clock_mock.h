// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_CLOCK_MOCK_H_
#define EXTRAS_TEST_DOUBLES_CLOCK_MOCK_H_

#include <gmock/gmock.h>
#include <supla/clock/clock.h>
#include <supla/control/hvac_base.h>

class ClockMock : public Supla::Clock {
 public:
  ClockMock() {}
  virtual ~ClockMock() {}

  MOCK_METHOD(bool, isReady, (), (override));
  MOCK_METHOD(int, getYear, (), (override));
  MOCK_METHOD(int, getMonth, (), (override));
  MOCK_METHOD(int, getDay, (), (override));
  MOCK_METHOD(int, getDayOfWeek, (), (override));
  MOCK_METHOD((enum Supla::DayOfWeek), getHvacDayOfWeek, (), (override));
  MOCK_METHOD(int, getHour, (), (override));
  MOCK_METHOD(int, getQuarter, (), (override));
  MOCK_METHOD(int, getMin, (), (override));
  MOCK_METHOD(int, getSec, (), (override));
  MOCK_METHOD(time_t, getTimeStamp, (), (override));

  MOCK_METHOD(void,
              parseLocaltimeFromServer,
              (TSDC_UserLocalTimeResult * result),
              (override));
};

#endif  // EXTRAS_TEST_DOUBLES_CLOCK_MOCK_H_
