// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_
#define EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_

#include <suplan/suplan_ports.h>

namespace Supla {
namespace SupLan {

class EspIdfUdpPort : public DatagramPort {
 public:
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

 private:
  int socket_;
  size_t maxDatagramPayload_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_SUPLAN_UDP_ESPIDF_H_
