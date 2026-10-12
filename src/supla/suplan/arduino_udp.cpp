// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "arduino_udp.h"
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
#include <supla/time.h>
namespace Supla {
namespace Device {
bool SupLanArduinoUdp::open() {
  if (open_ || !interfaceAddress()) return false;
#if defined(ARDUINO_ARCH_ESP8266)
  open_ = udp_.beginMulticast(
      IPAddress(interfaceAddress()), group(), kPort) != 0;
#else
  open_ = udp_.beginMulticast(group(), kPort) != 0;
#endif
  if (!open_) udp_.stop();
  return open_;
}
void SupLanArduinoUdp::close() {
  udp_.stop();
  open_ = false;
}
bool SupLanArduinoUdp::valid(const uint8_t *data, size_t length) const {
  return open_ && data && length && length <= maxDatagramPayload();
}
bool SupLanArduinoUdp::sendBody(const uint8_t *data, size_t length) {
  const bool written = udp_.write(data, length) == length;
  const bool sent = udp_.endPacket() == 1;
  return written && sent;
}
bool SupLanArduinoUdp::sendUnicast(const SupLan::Endpoint &endpoint,
    const uint8_t *data, size_t length) {
  if (!valid(data, length) ||
      !udp_.beginPacket(IPAddress(endpoint.address), endpoint.port))
    return false;
  return sendBody(data, length);
}
bool SupLanArduinoUdp::sendLocateMulticast(const uint8_t *data, size_t length) {
  if (!valid(data, length)) return false;
#if defined(ARDUINO_ARCH_ESP8266)
  if (!udp_.beginPacketMulticast(group(), kPort,
                                IPAddress(interfaceAddress()), 1)) return false;
#else
  if (!udp_.beginMulticastPacket()) return false;
#endif
  return sendBody(data, length);
}
int SupLanArduinoUdp::pollReceive(uint8_t *buffer, size_t capacity,
                                  SupLan::Endpoint *endpoint) {
  if (!open_ || !buffer || !endpoint) return -1;
  const int size = udp_.parsePacket();
  if (!size) return 0;
  if (size < 0 || static_cast<size_t>(size) > capacity ||
      static_cast<size_t>(size) > maxDatagramPayload()) {
    while (udp_.available()) udp_.read();
    return -1;
  }
  endpoint->address = static_cast<uint32_t>(udp_.remoteIP());
  endpoint->port = udp_.remotePort();
  const int read = udp_.read(buffer, size);
  return read == size ? read : -1;
}
uint32_t SupLanArduinoUdp::nowMs() const { return millis(); }
}  // namespace Device
}  // namespace Supla
#endif
