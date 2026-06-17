/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
   */

#ifndef SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_
#define SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_

#include <stdint.h>
#include <supla/suplet/channel_allocator.h>
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
                             uint16_t definitionVersion,
                             const ChannelAllocator &occupied);
  AssignmentResult validateJson(const char *json,
                                uint32_t definitionId,
                                uint16_t definitionVersion,
                                const ChannelAllocator &occupied) const;
  AssignmentResult remove(uint8_t instanceId);

 private:
  Manager *manager = nullptr;
  const Registry *registry = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_ASSIGNMENT_APPLIER_H_
