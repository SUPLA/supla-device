// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONDITION_GETTER_H_
#define SRC_SUPLA_CONDITION_GETTER_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {

class Element;

class ConditionGetter {
 public:
  virtual ~ConditionGetter() {}
  virtual double getValue(Supla::Element *element, bool *isValid) = 0;
 protected:
  TElectricityMeter_Measurement *getMeasurement(Supla::Element *element,
      _supla_int_t *measuredValues);
};


};  // namespace Supla

Supla::ConditionGetter *EmVoltage(int8_t phase = 0);
Supla::ConditionGetter *EmCurrent(int8_t phase = 0);
Supla::ConditionGetter *EmTotalCurrent();
Supla::ConditionGetter *EmPowerActiveW(int8_t phase = 0);
Supla::ConditionGetter *EmTotalPowerActiveW();
Supla::ConditionGetter *EmPowerApparentVA(int8_t phase = 0);
Supla::ConditionGetter *EmTotalPowerApparentVA();
Supla::ConditionGetter *EmPowerReactiveVar(int8_t phase = 0);
Supla::ConditionGetter *EmTotalPowerReactiveVar();

/**
 * Returns a getter for the remaining countdown timer time, in seconds.
 *
 * The getter is valid only while the source element has an active countdown
 * timer. Inactive or expired timers are reported as invalid instead of 0, so
 * threshold conditions like OnLess(60, CountdownTimerRemainingSec()) do not
 * fire when no timer is running.
 */
Supla::ConditionGetter *CountdownTimerRemainingSec();

#endif  // SRC_SUPLA_CONDITION_GETTER_H_
