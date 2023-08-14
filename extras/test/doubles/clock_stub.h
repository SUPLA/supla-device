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

#ifndef EXTRAS_TEST_DOUBLES_CLOCK_STUB_H_
#define EXTRAS_TEST_DOUBLES_CLOCK_STUB_H_

#include <supla/clock/clock.h>
#include <supla/control/hvac_base.h>

class ClockStub : public Supla::Clock {
 public:
  ClockStub() {}
  virtual ~ClockStub() {}

  bool isReady() override;
  int getYear() override;
  int getMonth() override;
  int getDay() override;
  int getDayOfWeek() override;
  int getHour() override;
  int getMin() override;
  int getSec() override;
  time_t getTimeStamp() override;

 protected:
  time_t now = 1672531200;  // 2023-01-01 00:00:00
};

#endif  // EXTRAS_TEST_DOUBLES_CLOCK_STUB_H_
