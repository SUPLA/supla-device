// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../condition.h"

class OnLessEqCond : public Supla::Condition {
 public:
  using Supla::Condition::Condition;

  bool condition(double val, bool isValid) {
    if (isValid) {
      return val <= threshold;
    }
    return false;
  }
};


Supla::Condition *OnLessEq(double threshold, bool useAlternativeMeasurement) {
  return new OnLessEqCond(threshold, useAlternativeMeasurement);
}

Supla::Condition *OnLessEq(double threshold, Supla::ConditionGetter *getter) {
  return new OnLessEqCond(threshold, getter);
}

