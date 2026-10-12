// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_OWNED_STACK_H_
#define SRC_SUPLA_SUPLAN_OWNED_STACK_H_
#ifndef ARDUINO_ARCH_AVR
#include <supla/suplan/bootstrap.h>
#include <supla/suplan/suplan_server_associations.h>
#include <supla/protocol/suplan_protocol.h>
#include <memory>
namespace Supla {
namespace Device {
template <class DefaultCrypto, class DefaultUdp>
class OwnedSupLanStack : public SupLanStack {
 public:
  OwnedSupLanStack(SuplaDeviceClass *device, uint8_t version)
      : protocol_(allocateSupLan<Protocol::SupLan>(
            device, &peers_, nullptr, 0)),
        runtime_(&crypto_, &udp_, protocol_.get(), &peers_,
                  {SupLan::kNodeIdDevice, 0}, version) {}
  ~OwnedSupLanStack() override {
    // Credentials/peer slots are cleared while Runtime and Application live.
    // Then unregister/destroy ProtocolLayer before its transport dependencies.
    if (protocol_) {
      protocol_->disconnect();
      protocol_->attachServerAssociations(nullptr);
    }
    associations_.reset();
    if (protocol_) protocol_->attachRuntime(nullptr);
    protocol_.reset();
    udp_.close();
  }
  bool initialize() {
    if (!protocol_ || !verifySupLanCrypto(&crypto_)) return false;
    associations_.reset(allocateSupLan<ServerAssociations>(&peers_, &runtime_));
    if (!associations_) return false;
    protocol_->attachRuntime(&runtime_);
    protocol_->attachServerAssociations(associations_.get());
    protocol_->attachTransportLifecycle(&udp_);
    return ready();
  }
  bool ready() const override {
    return protocol_ && protocol_->isSupLanServerCapable();
  }

 private:
  DefaultCrypto crypto_;
  DefaultUdp udp_;
  SupLan::PeerTable peers_;
  std::unique_ptr<Protocol::SupLan> protocol_;
  SupLan::Runtime runtime_;
  std::unique_ptr<ServerAssociations> associations_;
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_OWNED_STACK_H_
