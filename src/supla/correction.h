// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CORRECTION_H_
#define SRC_SUPLA_CORRECTION_H_

#include <stdint.h>

namespace Supla {

class Correction {
 public:
  static void add(uint8_t channelNumber,
       double correction,
       bool forSecondaryValue = false);
  static double get(uint8_t channelNumber, bool forSecondaryValue = false);
  static Correction *getInstance(uint8_t channelNumber,
                                 bool forSecondaryValue = false);
  static void clear();

 protected:
  Correction(uint8_t channelNumber, double correction, bool forSecondaryValue);
  ~Correction();

  static Correction *first;

  double correction = 0;
  Correction *next = nullptr;

  uint8_t channelNumber = 0;
  bool forSecondaryValue = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_CORRECTION_H_
