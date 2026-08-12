// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_CAPABILITY_REGISTRY_H_
#define SRC_SUPLA_SUPLET_CAPABILITY_REGISTRY_H_

#include <stdint.h>
#include <supla/suplet/definition.h>

namespace Supla {
namespace Suplet {

class CapabilityRegistry {
 public:
  ~CapabilityRegistry();

  bool add(const Capability &capability);
  void clear();

  uint8_t getCount() const;
  bool getCapability(uint8_t index, Capability *capability) const;

 private:
  struct Entry {
    Capability capability = {};
    Entry *next = nullptr;
  };

  bool contains(Category category, Kind kind, uint8_t handlerVersion) const;

  Entry *first = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_CAPABILITY_REGISTRY_H_
