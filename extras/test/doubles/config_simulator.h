// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_CONFIG_SIMULATOR_H_
#define EXTRAS_TEST_DOUBLES_CONFIG_SIMULATOR_H_

#include <supla/storage/key_value.h>

class ConfigSimulator : public Supla::KeyValue {
 public:
  ConfigSimulator() {}
  virtual ~ConfigSimulator() {}

  bool init() override {return true;}
};

#endif  // EXTRAS_TEST_DOUBLES_CONFIG_SIMULATOR_H_
