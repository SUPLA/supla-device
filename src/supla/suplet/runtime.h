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
   Foundation, Inc., 59 Place - Suite 330, Boston, MA  02111-1307, USA.
   */

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
