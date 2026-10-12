// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_NETWORK_TRANSPORT_H_
#define SRC_SUPLA_SUPLAN_NETWORK_TRANSPORT_H_
#ifndef ARDUINO_ARCH_AVR
#include <supla/protocol/suplan_protocol.h>

namespace Supla {
namespace Device {
// Common network lifecycle, independent of WAN/SRPC. Concrete ports provide
// only socket operations. One fingerprint detects address/interface changes.
class SupLanNetworkTransport : public Protocol::SupLanTransportLifecycle {
 public:
  bool networkReady(uint32_t nowMs) override;
 protected:
  uint32_t interfaceAddress() const { return address_; }
 private:
  uint32_t fingerprint_ = 0;
  uint32_t address_ = 0;
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_NETWORK_TRANSPORT_H_
