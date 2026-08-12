// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../condition.h"

class OnLessCond : public Supla::Condition {
 public:
  using Supla::Condition::Condition;

  bool condition(double val, bool isValid) {
    if (isValid) {
      return val < threshold;
    }
    return false;
  }
};


Supla::Condition *OnLess(double threshold, bool useAlternativeMeasurement) {
  return new OnLessCond(threshold, useAlternativeMeasurement);
}

Supla::Condition *OnLess(double threshold, Supla::ConditionGetter *getter) {
  return new OnLessCond(threshold, getter);
}


