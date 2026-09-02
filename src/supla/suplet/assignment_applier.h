// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_
#define SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_

#include <stdint.h>
#include <supla/suplet/manager.h>
#include <supla/suplet/registry.h>

namespace Supla {
namespace Suplet {

enum class AssignmentResult : uint8_t {
  Applied = 0,
  Removed = 1,
  InvalidArgument = 2,
  DefinitionNotSupported = 3,
  InvalidConfig = 4,
  StorageError = 5,
  ResourceLimitExceeded = 6,
  InstanceLimitExceeded = 7,
  ChannelLimitExceeded = 8,
};

class AssignmentApplier {
 public:
  AssignmentApplier(Manager *manager, const Registry *registry);

  AssignmentResult applyJson(const char *json,
                             uint32_t definitionId,
                             uint16_t definitionVersion);
  AssignmentResult validateJson(const char *json,
                                uint32_t definitionId,
                                uint16_t definitionVersion) const;
  AssignmentResult remove(uint8_t instanceId);

 private:
  Manager *manager = nullptr;
  const Registry *registry = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_
