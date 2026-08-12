// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

