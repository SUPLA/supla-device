// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../condition.h"

class OnInvalidCond : public Supla::Condition {
 public:
  explicit OnInvalidCond(bool useAlternativeMeasurement)
      : Supla::Condition(0, useAlternativeMeasurement) {
  }

  explicit OnInvalidCond(Supla::ConditionGetter *getter)
      : Supla::Condition(0, getter) {
  }

  bool condition(double val, bool isValid) {
    (void)(val);
    return !isValid;
  }
};

Supla::Condition *OnInvalid(bool useAlternativeMeasurement) {
  return new OnInvalidCond(useAlternativeMeasurement);
}

Supla::Condition *OnInvalid(Supla::ConditionGetter *getter) {
  return new OnInvalidCond(getter);
}
