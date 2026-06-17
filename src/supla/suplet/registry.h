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

#ifndef SRC_SUPLA_SUPLET_REGISTRY_H_
#define SRC_SUPLA_SUPLET_REGISTRY_H_

#include <stdint.h>
#include <supla/suplet/definition.h>

namespace Supla {
namespace Suplet {

class Registry {
 public:
  bool add(const Definition *definition,
           uint8_t maxInstances = 1,
           bool supportsDownloadedDefinition = false);
  bool remove(uint32_t definitionId, uint16_t definitionVersion = 0);
  void clear();

  uint8_t getCount() const;
  const Definition *findDefinition(uint32_t definitionId,
                                   uint16_t definitionVersion = 0) const;
  bool getCapability(uint8_t index, Capability *capability) const;
  bool getCapability(uint32_t definitionId,
                     uint16_t definitionVersion,
                     Capability *capability) const;

 private:
  struct Entry {
    const Definition *definition = nullptr;
    uint8_t maxInstances = 1;
    uint8_t supportsDownloadedDefinition = 0;
    Entry *next = nullptr;
  };

  bool contains(uint32_t definitionId, uint16_t definitionVersion) const;
  Entry *first = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_REGISTRY_H_
