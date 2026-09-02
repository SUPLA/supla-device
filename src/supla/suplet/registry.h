// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_REGISTRY_H_
#define SRC_SUPLA_SUPLET_REGISTRY_H_

#include <stdint.h>
#include <supla/suplet/definition.h>

namespace Supla {
namespace Suplet {

class Registry {
 public:
  ~Registry();

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
