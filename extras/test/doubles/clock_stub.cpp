// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

