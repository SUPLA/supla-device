// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_ARDUINO_UDP_H_
#define SRC_SUPLA_SUPLAN_ARDUINO_UDP_H_
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#include <WiFiUdp.h>
#include <supla/suplan/network_transport.h>
#include <suplan/suplan_fragment.h>

namespace Supla {
namespace Device {
class SupLanArduinoUdp : public SupLan::DatagramPort,
                         public SupLanNetworkTransport {
 public:
  ~SupLanArduinoUdp() override { close(); }
  bool isOpen() const override { return open_; }
  bool open() override;
  void close() override;
  bool sendUnicast(const SupLan::Endpoint &endpoint, const uint8_t *data,
                   size_t length) override;
  bool sendLocateMulticast(const uint8_t *data, size_t length) override;
  int pollReceive(uint8_t *buffer, size_t capacity,
                  SupLan::Endpoint *endpoint) override;
  size_t maxDatagramPayload() const override {
    return SupLan::kMaxDatagramPayload;
  }
  uint32_t nowMs() const override;
 private:
  static constexpr uint16_t kPort = 2016;
  static IPAddress group() { return IPAddress(239, 255, 201, 6); }
  bool valid(const uint8_t *data, size_t length) const;
  bool sendBody(const uint8_t *data, size_t length);
  WiFiUDP udp_;
  bool open_ = false;
};
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_ARDUINO_UDP_H_
