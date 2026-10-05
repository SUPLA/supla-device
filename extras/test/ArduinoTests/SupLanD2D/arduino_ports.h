// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_ARDUINOTESTS_SUPLAND2D_ARDUINO_PORTS_H_
#define EXTRAS_TEST_ARDUINOTESTS_SUPLAND2D_ARDUINO_PORTS_H_

// Test adapter only. Peer provisioning remains the existing PoC fixture.
#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#include <lwip/igmp.h>
#include <suplan/suplan_crypto_bearssl.h>
using ArduinoCrypto = Supla::SupLan::BearSslCryptoPort;
#else
#include <WiFi.h>
#include <suplan/suplan_crypto_mbedtls.h>
using ArduinoCrypto = Supla::SupLan::MbedTlsCryptoPort;
#endif
#include <WiFiUdp.h>
#include <string.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/network/network.h>
#include <supla/tools.h>
#include <suplan/suplan_fragment.h>
#include <suplan/suplan_ports.h>


class ArduinoUdp : public Supla::SupLan::DatagramPort,
                   public Supla::Protocol::SupLanTransportLifecycle {
 public:
  uint32_t rxUnicast = 0;
  uint32_t rxMulticast = 0;
  uint32_t rxOversize = 0;

  bool networkReady(uint32_t) override {
    return enabled && Supla::Network::IsReady() &&
        static_cast<uint32_t>(WiFi.localIP()) != 0;
  }
  bool isOpen() const override { return opened; }
  bool open() override {
    close();
    #if defined(ARDUINO_ARCH_ESP8266)
    const bool joined = multicast.beginMulticast(
        WiFi.localIP(), IPAddress(239, 255, 201, 6), 2016);
#else
    const bool joined = multicast.beginMulticast(
        IPAddress(239, 255, 201, 6), 2016);
#endif
    joinedMulticast = joined;
    joinedAddress = WiFi.localIP();
    opened = joined && unicast.begin(2017);
    if (!opened) {
      close();
    }
    return opened;
  }
  void close() override {
    unicast.stop();
    multicast.stop();
#if defined(ARDUINO_ARCH_ESP8266)
    // WiFiUDP::stop() does not leave its IGMP membership on ESP8266.
    if (joinedMulticast) {
      IPAddress group(239, 255, 201, 6);
      igmp_leavegroup(joinedAddress, group);
    }
#endif
    joinedMulticast = false;
    opened = false;
  }
  void setEnabled(bool value) {
    enabled = value;
    if (!value) {
      close();
    }
    resetOpenRetry();
  }
  bool sendUnicast(const Supla::SupLan::Endpoint &endpoint,
                   const uint8_t *data, size_t length) override {
    if (!opened || !data || !length || length > maxDatagramPayload() ||
        !unicast.beginPacket(IPAddress(endpoint.address), endpoint.port)) {
      return false;
    }
    const bool written = unicast.write(data, length) == length;
    return unicast.endPacket() == 1 && written;
  }
  bool sendLocateMulticast(const uint8_t *data, size_t length) override {
    if (!opened || !data || !length || length > maxDatagramPayload()) {
      return false;
    }
#if defined(ARDUINO_ARCH_ESP8266)
    const bool started = unicast.beginPacketMulticast(
        IPAddress(239, 255, 201, 6), 2016, WiFi.localIP(), 1);
#else
    const bool started = unicast.beginPacket(IPAddress(239, 255, 201, 6), 2016);
#endif
    if (!started) {
      return false;
    }
    const bool written = unicast.write(data, length) == length;
    return unicast.endPacket() == 1 && written;
  }
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Supla::SupLan::Endpoint *endpoint) override {
    if (!opened || !buffer || !capacity || !endpoint) {
      return -1;
    }
    WiFiUDP *sockets[] = {&unicast, &multicast};
    for (auto *socket : sockets) {
      const int length = socket->parsePacket();
      if (length <= 0) {
        continue;
      }
      if (socket == &multicast) {
        ++rxMulticast;
      } else {
        ++rxUnicast;
      }
      if (static_cast<size_t>(length) > capacity ||
          static_cast<size_t>(length) > maxDatagramPayload()) {
        ++rxOversize;
        uint8_t discard[32];
        while (socket->available() > 0) {
          if (socket->read(discard, sizeof(discard)) <= 0) {
            break;
          }
        }
        return -1;
      }
      endpoint->address = static_cast<uint32_t>(socket->remoteIP());
      endpoint->port = socket->remotePort();
      const int received = socket->read(buffer, capacity);
      return received == length ? received : -1;
    }
    return 0;
  }
  size_t maxDatagramPayload() const override {
    return Supla::SupLan::kMaxDatagramPayload;
  }
  uint32_t nowMs() const override { return millis(); }

 private:
  WiFiUDP unicast;
  WiFiUDP multicast;
  IPAddress joinedAddress;
  bool joinedMulticast = false;
  bool opened = false;
  bool enabled = true;
};

#endif  // EXTRAS_TEST_ARDUINOTESTS_SUPLAND2D_ARDUINO_PORTS_H_
