// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
