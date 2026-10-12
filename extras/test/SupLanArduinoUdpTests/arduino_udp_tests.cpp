// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <supla/network/network.h>
#include <supla/suplan/arduino_udp.h>

UdpState udpState;
Supla::Network *Supla::Network::first = nullptr;
uint32_t millis() { return 1000; }
namespace {
class SupLanArduinoUdp : public testing::Test {
 protected:
  void SetUp() override {
    udpState = {};
    Supla::Network::first = &network;
    network.ready = true;
    network.ip = static_cast<uint32_t>(IPAddress(192, 168, 0, 40));
  }
  void TearDown() override { Supla::Network::first = nullptr; }
  Supla::Network network;
};
TEST_F(SupLanArduinoUdp, JoinAndNetworkLossKeepSocketLifecycleBounded) {
  Supla::Device::SupLanArduinoUdp port;
  ASSERT_TRUE(port.networkReady(0));
  ASSERT_TRUE(port.openIfDue(0));
  EXPECT_EQ(udpState.joins, 1);
  EXPECT_EQ(udpState.port, 2016);
  EXPECT_EQ(udpState.group,
            static_cast<uint32_t>(IPAddress(239, 255, 201, 6)));
  EXPECT_FALSE(port.open());
  network.ip += 1;
  EXPECT_TRUE(port.networkReady(1));
  EXPECT_FALSE(port.isOpen());
  EXPECT_EQ(udpState.closes, 1);
  EXPECT_FALSE(port.openIfDue(999));
  EXPECT_TRUE(port.openIfDue(1000));
}
TEST_F(SupLanArduinoUdp, NoIpAndFailedJoinDoNotSpinOrClaimOpen) {
  Supla::Device::SupLanArduinoUdp port;
  network.ip = 0;
  EXPECT_FALSE(port.networkReady(0));
  EXPECT_FALSE(port.open());
  EXPECT_EQ(udpState.joins, 0);
  network.ip = 1;
  ASSERT_TRUE(port.networkReady(1));
  udpState.failJoin = true;
  EXPECT_FALSE(port.openIfDue(1));
  EXPECT_FALSE(port.isOpen());
  EXPECT_FALSE(port.openIfDue(1000));
  EXPECT_EQ(udpState.joins, 1);
  EXPECT_EQ(udpState.closes, 1);
  udpState.failJoin = false;
  EXPECT_TRUE(port.openIfDue(1001));
}
TEST_F(SupLanArduinoUdp, MulticastAndUnicastUseCorrectInterfaceAndPorts) {
  Supla::Device::SupLanArduinoUdp port;
  ASSERT_TRUE(port.networkReady(0));
  ASSERT_TRUE(port.openIfDue(0));
  uint8_t bytes[250] = {1, 2, 3};
  ASSERT_TRUE(port.sendLocateMulticast(bytes, sizeof(bytes)));
  EXPECT_EQ(udpState.destination, udpState.group);
  EXPECT_EQ(udpState.destinationPort, 2016);
#if defined(ARDUINO_ARCH_ESP8266)
  EXPECT_EQ(udpState.interfaceIp, network.ip);
  EXPECT_EQ(udpState.ttl, 1);
#endif
  Supla::SupLan::Endpoint endpoint = {network.ip + 1, 2017};
  ASSERT_TRUE(port.sendUnicast(endpoint, bytes, sizeof(bytes)));
  EXPECT_EQ(udpState.destination, endpoint.address);
  EXPECT_EQ(udpState.destinationPort, endpoint.port);
  EXPECT_EQ(udpState.transmitLength, sizeof(bytes));
  EXPECT_EQ(udpState.transmitted[0], 1);
  EXPECT_EQ(port.nowMs(), 1000u);
}
TEST_F(SupLanArduinoUdp, RejectsOversizeAndWriteSendFailures) {
  Supla::Device::SupLanArduinoUdp port;
  ASSERT_TRUE(port.networkReady(0));
  ASSERT_TRUE(port.openIfDue(0));
  uint8_t bytes[251] = {};
  EXPECT_FALSE(port.sendLocateMulticast(bytes, sizeof(bytes)));
  EXPECT_EQ(udpState.begins, 0);
  udpState.partialWrite = true;
  EXPECT_FALSE(port.sendLocateMulticast(bytes, 10));
  udpState.partialWrite = false;
  udpState.failEndPacket = true;
  EXPECT_FALSE(port.sendLocateMulticast(bytes, 10));
  udpState.failEndPacket = false;
  udpState.failBeginPacket = true;
  EXPECT_FALSE(port.sendLocateMulticast(bytes, 10));
}
TEST_F(SupLanArduinoUdp, ReceivePreservesEndpointAndDrainsOversizePacket) {
  Supla::Device::SupLanArduinoUdp port;
  ASSERT_TRUE(port.networkReady(0));
  ASSERT_TRUE(port.openIfDue(0));
  Supla::SupLan::Endpoint endpoint = {};
  uint8_t bytes[250] = {};
  EXPECT_EQ(port.pollReceive(bytes, sizeof(bytes), &endpoint), 0);
  udpState.receiveSize = udpState.remaining = 251;
  EXPECT_EQ(port.pollReceive(bytes, sizeof(bytes), &endpoint), -1);
  EXPECT_EQ(udpState.remaining, 0);
  udpState.receiveSize = udpState.remaining = 10;
  EXPECT_EQ(port.pollReceive(bytes, sizeof(bytes), &endpoint), 10);
  EXPECT_EQ(bytes[0], 9);
  EXPECT_EQ(endpoint.port, 2017);
  EXPECT_EQ(endpoint.address,
            static_cast<uint32_t>(IPAddress(192, 168, 0, 41)));
}
}  // namespace
