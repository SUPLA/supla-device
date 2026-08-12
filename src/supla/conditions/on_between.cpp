// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../condition.h"

class OnBetweenCond : public Supla::Condition {
 public:
  OnBetweenCond(double threshold1,
                double threshold2,
                bool useAlternativeMeasurement)
      : Supla::Condition(threshold1, useAlternativeMeasurement),
        threshold2(threshold2) {
  }

  OnBetweenCond(double threshold1,
                double threshold2,
                Supla::ConditionGetter *getter)
      : Supla::Condition(threshold1, getter), threshold2(threshold2) {
  }

  bool condition(double val, bool isValid) {
    if (isValid) {
      return val > threshold && val < threshold2;
    }
    return false;
  }

  double threshold2;
};

Supla::Condition *OnBetween(double threshold1,
                            double threshold2,
                            bool useAlternativeMeasurement) {
  return new OnBetweenCond(threshold1, threshold2, useAlternativeMeasurement);
}

Supla::Condition *OnBetween(double threshold1,
                            double threshold2,
                            Supla::ConditionGetter *getter) {
  return new OnBetweenCond(threshold1, threshold2, getter);
}
