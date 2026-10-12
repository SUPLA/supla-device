// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_DOUBLES_SUPLAN_BOOTSTRAP_PORT_H_
#define EXTRAS_TEST_DOUBLES_SUPLAN_BOOTSTRAP_PORT_H_
#include <supla/suplan/network_transport.h>
#include <suplan/suplan_fragment.h>
#include <Arduino.h>

class BootstrapUdp : public Supla::SupLan::DatagramPort,
                     public Supla::Device::SupLanNetworkTransport {
 public:
  ~BootstrapUdp() override { close(); }
  bool isOpen() const override { return opened; }
  bool open() override {
    ++attempts;
    opened = !failOpen && interfaceAddress();
    return opened;
  }
  void close() override {
    if (opened) ++closes;
    opened = false;
  }
  bool sendUnicast(const Supla::SupLan::Endpoint &, const uint8_t *,
                   size_t) override { return false; }
  bool sendLocateMulticast(const uint8_t *, size_t) override { return false; }
  int pollReceive(uint8_t *, size_t, Supla::SupLan::Endpoint *) override {
    return 0;
  }
  size_t maxDatagramPayload() const override {
    return Supla::SupLan::kMaxDatagramPayload;
  }
  uint32_t nowMs() const override { return millis(); }
  int attempts = 0;
  int closes = 0;
  bool failOpen = false;
  bool opened = false;
};
#endif  // EXTRAS_TEST_DOUBLES_SUPLAN_BOOTSTRAP_PORT_H_
