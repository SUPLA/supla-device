// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_JSON_INSTANCE_CONFIG_H_
#define SRC_SUPLA_SUPLET_JSON_INSTANCE_CONFIG_H_

#include <stdint.h>
#include <supla/suplet/definition.h>
#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

class JsonInstanceConfigParser {
 public:
  static bool parse(const char *json,
                    const Definition &definition,
                    InstanceRecord *output);
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_JSON_INSTANCE_CONFIG_H_
