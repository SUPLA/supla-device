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

#include "../condition.h"

class OnEqualCond : public Supla::Condition {
 public:
  using Supla::Condition::Condition;

  bool condition(double val, bool isValid) {
    if (isValid) {
      return val == threshold;
    }
    return false;
  }
};


Supla::Condition *OnEqual(double threshold, bool useAlternativeMeasurement) {
  return new OnEqualCond(threshold, useAlternativeMeasurement);
}

Supla::Condition *OnEqual(double threshold, Supla::ConditionGetter *getter) {
  return new OnEqualCond(threshold, getter);
}

