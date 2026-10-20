// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_BOOTSTRAP_H_
#define SRC_SUPLA_SUPLAN_BOOTSTRAP_H_

#include <stdint.h>

#ifndef ARDUINO_ARCH_AVR
#include <new>
#include <utility>
class SuplaDeviceClass;
namespace Supla {
namespace SupLan { class CryptoPort; }
namespace Device {
// Private SDK ownership boundary. Factories are linked by enableSupLan,
// not by begin(): firmware without opt-in does not retain platform pools.
class SupLanStack {
 public:
  virtual ~SupLanStack() = default;
  virtual bool ready() const = 0;
};
using SupLanFactory = SupLanStack *(*)(SuplaDeviceClass *, uint8_t);
bool requestSupLan(SuplaDeviceClass *device, SupLanFactory factory);
bool startSupLan(SuplaDeviceClass *device, uint8_t version);
void releaseSupLan(SuplaDeviceClass *device);
bool hasSupLanServerStack(SuplaDeviceClass *device);
bool supLanRequested(SuplaDeviceClass *device);
bool verifySupLanCrypto(SupLan::CryptoPort *crypto);
#ifdef SUPLA_TEST
void setSupLanFactoryForTest(SupLanFactory factory);
void failSupLanAllocationAfter(int successfulAllocations);
bool supLanAllocationAllowed();
#else
inline bool supLanAllocationAllowed() { return true; }
#endif
template <class T, class... Args>
T *allocateSupLan(Args &&...args) {
  if (!supLanAllocationAllowed()) return nullptr;
  return new (std::nothrow) T(std::forward<Args>(args)...);
}
}  // namespace Device
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_SUPLAN_BOOTSTRAP_H_
