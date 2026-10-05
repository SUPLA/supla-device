// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <suplan_udp_linux.h>
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstring>

namespace {
sockaddr_in addresses[2];
ifaddrs interfaces[2];
char ethernetName[] = "eth-test";
char wifiName[] = "wifi-test";
int nextSocket = 40;
int closedSockets = 0;
int pinCalls = 0;
uint32_t pinnedIndex = 0;
bool failPin = false;
bool failEnumeration = false;

class LinuxUdpInterfaceTests : public ::testing::Test {
 protected:
  void SetUp() override {
    std::memset(addresses, 0, sizeof(addresses));
    std::memset(interfaces, 0, sizeof(interfaces));
    inet_pton(AF_INET, "192.168.0.9", &addresses[0].sin_addr);
    inet_pton(AF_INET, "192.168.0.177", &addresses[1].sin_addr);
    for (size_t i = 0; i < 2; ++i) {
      addresses[i].sin_family = AF_INET;
      interfaces[i].ifa_addr = reinterpret_cast<sockaddr *>(&addresses[i]);
      interfaces[i].ifa_flags = IFF_UP | IFF_RUNNING | IFF_MULTICAST;
    }
    interfaces[0].ifa_name = ethernetName;
    interfaces[0].ifa_next = &interfaces[1];
    interfaces[1].ifa_name = wifiName;
    nextSocket = 40;
    closedSockets = 0;
    pinCalls = 0;
    pinnedIndex = 0;
    failPin = false;
    failEnumeration = false;
  }
};
}  // namespace

// Link wrappers exercise the actual Linux port without real sockets, network
// changes, root permissions or dependence on the host interface configuration.
extern "C" {
int __wrap_socket(int, int, int) { return nextSocket++; }
int __wrap_bind(int, const sockaddr *, socklen_t) { return 0; }
int __wrap_fcntl(int, int, ...) { return 0; }
int __wrap_close(int) {
  ++closedSockets;
  return 0;
}
int __wrap_getifaddrs(ifaddrs **output) {
  if (failEnumeration) {
    errno = EIO;
    return -1;
  }
  *output = interfaces;
  return 0;
}
void __wrap_freeifaddrs(ifaddrs *) {}
unsigned int __wrap_if_nametoindex(const char *name) {
  return std::strcmp(name, wifiName) == 0 ? 17 : 4;
}
int __wrap_setsockopt(int, int level, int option, const void *value,
                     socklen_t length) {
  if (level == IPPROTO_IP && option == IP_UNICAST_IF) {
    ++pinCalls;
    if (length != sizeof(uint32_t)) {
      errno = EINVAL;
      return -1;
    }
    uint32_t index = 0;
    std::memcpy(&index, value, sizeof(index));
    pinnedIndex = ntohl(index);
    if (failPin) {
      errno = EPERM;
      return -1;
    }
  }
  return 0;
}
}

TEST_F(LinuxUdpInterfaceTests, ExplicitWifiPinsUnicastToWifiIndex) {
  Supla::SupLan::LinuxUdpPort port;
  ASSERT_TRUE(port.open("192.168.0.177", 2018, 250));
  EXPECT_EQ(pinCalls, 1);
  EXPECT_EQ(pinnedIndex, 17U);
  EXPECT_EQ(port.multicastInterfaceCount(), 1U);
  EXPECT_EQ(port.joinedMulticastInterfaceCount(), 1U);
}

TEST_F(LinuxUdpInterfaceTests, ExplicitEthernetPinsUnicastToEthernetIndex) {
  Supla::SupLan::LinuxUdpPort port;
  ASSERT_TRUE(port.open("192.168.0.9", 2018, 250));
  EXPECT_EQ(pinCalls, 1);
  EXPECT_EQ(pinnedIndex, 4U);
}

TEST_F(LinuxUdpInterfaceTests, AutomaticSelectionKeepsDefaultUnicastRouting) {
  Supla::SupLan::LinuxUdpPort port;
  ASSERT_TRUE(port.open("0.0.0.0", 2018, 250));
  EXPECT_EQ(pinCalls, 0);
  EXPECT_EQ(port.multicastInterfaceCount(), 2U);
}

TEST_F(LinuxUdpInterfaceTests, FailedExplicitPinClosesBothSockets) {
  failPin = true;
  Supla::SupLan::LinuxUdpPort port;
  EXPECT_FALSE(port.open("192.168.0.177", 2018, 250));
  EXPECT_FALSE(port.isOpen());
  EXPECT_EQ(closedSockets, 2);
}

TEST_F(LinuxUdpInterfaceTests, FailedInterfaceEnumerationClosesBothSockets) {
  failEnumeration = true;
  Supla::SupLan::LinuxUdpPort port;
  EXPECT_FALSE(port.open("192.168.0.177", 2018, 250));
  EXPECT_FALSE(port.isOpen());
  EXPECT_EQ(closedSockets, 2);
}
