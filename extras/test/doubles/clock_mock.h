/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

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
