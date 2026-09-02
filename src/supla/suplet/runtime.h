// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_RUNTIME_H_
#define SRC_SUPLA_SUPLET_RUNTIME_H_

#include <stdint.h>
#include <supla/element.h>
#include <supla/suplet/definition.h>
#include <supla/suplet/storage.h>

namespace Supla {
namespace Suplet {

class Runtime {
 public:
  static bool validateDefinition(const Definition &definition);
  static Supla::Element *createElement(const ChannelDefinition &channel,
                                       const InstanceRecord &instance);
  static Supla::Element *createElement(const Definition &definition,
                                       const ChannelDefinition &channel,
                                       const InstanceRecord &instance);
  static bool createElements(const Definition &definition,
                             const InstanceRecord &instance,
                             Supla::Element **created,
                             uint8_t createdSize,
                             ChannelMap *createdChannelMap = nullptr);
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_RUNTIME_H_
