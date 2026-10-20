// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_DOUBLES_CONFIG_KEY_COLLECTOR_H_
#define EXTRAS_TEST_DOUBLES_CONFIG_KEY_COLLECTOR_H_
#include <supla/storage/config.h>
#include <map>
#include <string>

struct ConfigKeyCollector {
  static bool collect(const char *key, Supla::ConfigValueType type, void *ctx) {
    auto self = static_cast<ConfigKeyCollector *>(ctx);
    ++self->visits;
    self->keys.emplace(key, type);
    return !self->stopAfter || self->visits < self->stopAfter;
  }
  std::map<std::string, Supla::ConfigValueType> keys;
  int visits = 0;
  int stopAfter = 0;
};
#endif  // EXTRAS_TEST_DOUBLES_CONFIG_KEY_COLLECTOR_H_
