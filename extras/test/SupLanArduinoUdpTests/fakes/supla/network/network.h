// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_SUPLA_NETWORK_NETWORK_H_
#define EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_SUPLA_NETWORK_NETWORK_H_
#include <stdint.h>
namespace Supla {
class Network {
 public:
  static Network *FirstInstance() { return first; }
  static Network *NextInstance(Network *current) { return current->next; }
  bool isReady() const { return ready; }
  uint32_t getIP() const { return ip; }
  static Network *first;
  Network *next = nullptr;
  bool ready = false;
  uint32_t ip = 0;
};
}  // namespace Supla
#endif  // EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_SUPLA_NETWORK_NETWORK_H_
