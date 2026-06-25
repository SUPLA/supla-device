/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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
