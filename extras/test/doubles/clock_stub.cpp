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

#include "clock_stub.h"
#include "Arduino.h"

#include <time.h>

/*  bool isReady() override;
  int getYear() override;
  int getMonth() override;
  int getDay() override;
  int getDayOfWeek() override;
  enum DayOfWeek getHvacDayOfWeek() override;
  int getHour() override;
  int getQuarter() override;
  int getMin() override;
  int getSec() override;
  time_t getTimeStamp() override;
  */

bool ClockStub::isReady() {
  return millis() > 0;
}

int ClockStub::getYear() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_year + 1900;
}

int ClockStub::getMonth() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_mon + 1;
}

int ClockStub::getDay() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_mday;
}

int ClockStub::getDayOfWeek() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_wday + 1;
}

int ClockStub::getHour() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_hour;
}

int ClockStub::getMin() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_min;
}

int ClockStub::getSec() {
  time_t t = now + (millis() / 1000);
  struct tm tm;
  gmtime_r(&t, &tm);
  return tm.tm_sec;
}

time_t ClockStub::getTimeStamp() {
  return now + (millis() / 1000);
}

