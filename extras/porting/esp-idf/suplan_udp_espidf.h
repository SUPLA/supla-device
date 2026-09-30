// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_
#define EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_

#include <suplan/suplan_ports.h>

namespace Supla {
namespace SupLan {

class EspIdfUdpPort : public DatagramPort {
 public:
  static const size_t kMaxMulticastInterfaces = 4;

  EspIdfUdpPort();
  ~EspIdfUdpPort() override;

  bool open(uint16_t port, size_t maxDatagramPayload);
  void close();
  bool isOpen() const;
  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                   size_t length) override;
  bool sendLocateMulticast(const uint8_t *data, size_t length) override;
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override;
  size_t maxDatagramPayload() const override;
  uint32_t nowMs() const override;
  size_t multicastInterfaceCount() const;
  size_t joinedMulticastInterfaceCount() const;

 private:
  bool refreshMulticastInterfaces();

  int socket_;
  int discoverySocket_;
  size_t maxDatagramPayload_;
  uint32_t interfaceAddresses_[kMaxMulticastInterfaces];
  size_t interfaceCount_;
  uint32_t joinedAddresses_[kMaxMulticastInterfaces];
  size_t joinedCount_;
  uint32_t lastInterfaceRefreshMs_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_
