// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../condition.h"

class OnGreaterCond : public Supla::Condition {
 public:
  using Supla::Condition::Condition;

  bool condition(double val, bool isValid) {
    if (isValid) {
      return val > threshold;
    }
    return false;
  }
};


Supla::Condition *OnGreater(double threshold, bool useAlternativeMeasurement) {
  return new OnGreaterCond(threshold, useAlternativeMeasurement);
}

Supla::Condition *OnGreater(double threshold, Supla::ConditionGetter *getter) {
  return new OnGreaterCond(threshold, getter);
}


