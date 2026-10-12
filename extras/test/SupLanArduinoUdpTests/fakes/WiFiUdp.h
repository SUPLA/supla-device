// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_WIFIUDP_H_
#define EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_WIFIUDP_H_
#include <stddef.h>
#include <stdint.h>
#include <string.h>
class IPAddress {
 public:
  explicit IPAddress(uint32_t raw = 0) : raw_(raw) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
      : raw_(a | (uint32_t{b} << 8) | (uint32_t{c} << 16) |
             (uint32_t{d} << 24)) {}
  explicit operator uint32_t() const { return raw_; }
 private:
  uint32_t raw_;
};
struct UdpState {
  bool failJoin = false;
  bool failBeginPacket = false;
  bool failEndPacket = false;
  bool partialWrite = false;
  int joins = 0;
  int closes = 0;
  int begins = 0;
  int ends = 0;
  uint32_t interfaceIp = 0;
  uint32_t group = 0;
  uint32_t destination = 0;
  uint16_t port = 0;
  uint16_t destinationPort = 0;
  int ttl = 0;
  int receiveSize = 0;
  int remaining = 0;
  uint8_t transmitted[250] = {};
  size_t transmitLength = 0;
};
extern UdpState udpState;
class WiFiUDP {
 public:
  uint8_t beginMulticast(IPAddress group, uint16_t port) {
    ++udpState.joins;
    udpState.group = static_cast<uint32_t>(group);
    udpState.port = port;
    return !udpState.failJoin;
  }
  uint8_t beginMulticast(IPAddress interfaceIp, IPAddress group,
                         uint16_t port) {
    udpState.interfaceIp = static_cast<uint32_t>(interfaceIp);
    return beginMulticast(group, port);
  }
  int beginPacket(IPAddress address, uint16_t port) {
    ++udpState.begins;
    udpState.destination = static_cast<uint32_t>(address);
    udpState.destinationPort = port;
    return !udpState.failBeginPacket;
  }
  int beginMulticastPacket() {
    return beginPacket(IPAddress(udpState.group), udpState.port);
  }
  int beginPacketMulticast(IPAddress group, uint16_t port,
                           IPAddress interfaceIp,
                           int ttl) {
    udpState.interfaceIp = static_cast<uint32_t>(interfaceIp);
    udpState.ttl = ttl;
    return beginPacket(group, port);
  }
  size_t write(const uint8_t *bytes, size_t length) {
    memcpy(udpState.transmitted, bytes, length);
    udpState.transmitLength = length;
    return udpState.partialWrite ? length - 1 : length;
  }
  int endPacket() { ++udpState.ends; return !udpState.failEndPacket; }
  void stop() { ++udpState.closes; }
  int parsePacket() { return udpState.receiveSize; }
  int available() { return udpState.remaining; }
  int read() { --udpState.remaining; return 1; }
  int read(uint8_t *buffer, size_t length) {
    memset(buffer, 9, length);
    udpState.receiveSize = udpState.remaining = 0;
    return length;
  }
  IPAddress remoteIP() const { return IPAddress(192, 168, 0, 41); }
  uint16_t remotePort() const { return 2017; }
};
#endif  // EXTRAS_TEST_SUPLANARDUINOUDPTESTS_FAKES_WIFIUDP_H_
