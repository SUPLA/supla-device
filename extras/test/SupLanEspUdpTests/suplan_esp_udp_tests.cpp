// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <suplan_udp_espidf.h>
#include <esp_netif.h>
#include <lwip/sockets.h>

#include <gtest/gtest.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

namespace {
struct Socket {
  bool closed;
  uint16_t port;
  bool joined;
  bool nonblocking;
};
Socket sockets[4];
int count;
int failBindPort;
int receivedPort;
int sendFd;
int sentPort;
esp_netif_t netif = {0};

class SupLanEspUdp : public ::testing::Test {
 protected:
  void SetUp() override {
    memset(sockets, 0, sizeof(sockets));
    count = 0;
    failBindPort = 0;
    receivedPort = 0;
    sendFd = -1;
    sentPort = 0;
  }
};
}  // namespace

esp_netif_t *esp_netif_next_unsafe(esp_netif_t *current) {
  return current == nullptr ? &netif : nullptr;
}
bool esp_netif_is_netif_up(esp_netif_t *) { return true; }
int esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *info) {
  info->ip.addr = htonl(0xC0A80028);
  return ESP_OK;
}
int64_t esp_timer_get_time() { return 0; }

int lwip_socket(int, int, int) { return count < 4 ? count++ : -1; }
int lwip_close(int fd) { sockets[fd].closed = true; return 0; }
int lwip_bind(int fd, const sockaddr *address, socklen_t) {
  const auto *local = reinterpret_cast<const sockaddr_in *>(address);
  if (ntohs(local->sin_port) == failBindPort) return -1;
  sockets[fd].port = ntohs(local->sin_port);
  return 0;
}
int lwip_setsockopt(int fd, int, int option, const void *, socklen_t) {
  if (option == IP_ADD_MEMBERSHIP) sockets[fd].joined = true;
  if (option == IP_DROP_MEMBERSHIP) sockets[fd].joined = false;
  return 0;
}
int lwip_fcntl(int fd, int command, int value) {
  if (command == F_SETFL) sockets[fd].nonblocking = (value & O_NONBLOCK) != 0;
  return 0;
}
int lwip_sendto(int fd, const void *, size_t length, int,
                const sockaddr *address, socklen_t) {
  sendFd = fd;
  sentPort = ntohs(reinterpret_cast<const sockaddr_in *>(address)->sin_port);
  return static_cast<int>(length);
}
int lwip_recvfrom(int fd, void *buffer, size_t capacity, int,
                  sockaddr *address, socklen_t *) {
  if (sockets[fd].port != receivedPort || capacity < 2) {
    errno = EAGAIN;
    return -1;
  }
  auto *source = reinterpret_cast<sockaddr_in *>(address);
  source->sin_addr.s_addr = htonl(0xC0A80009);
  source->sin_port = htons(2017);
  static_cast<uint8_t *>(buffer)[0] = 1;
  static_cast<uint8_t *>(buffer)[1] = 1;
  receivedPort = 0;
  return 2;
}

TEST_F(SupLanEspUdp, NonDefaultPortKeepsDiscoveryAndReplySourcePort) {
  Supla::SupLan::EspIdfUdpPort port;
  ASSERT_TRUE(port.open(2287, 250));
  ASSERT_EQ(count, 2);
  EXPECT_EQ(sockets[0].port, 2287);
  EXPECT_EQ(sockets[1].port, 2016);
  EXPECT_FALSE(sockets[0].joined);
  EXPECT_TRUE(sockets[1].joined);
  EXPECT_TRUE(sockets[0].nonblocking);
  EXPECT_TRUE(sockets[1].nonblocking);
  receivedPort = 2016;
  uint8_t buffer[50] = {};
  Supla::SupLan::Endpoint source = {};
  EXPECT_EQ(port.pollReceive(buffer, sizeof(buffer), &source), 2);
  EXPECT_EQ(source.port, 2017);
  ASSERT_TRUE(port.sendUnicast(source, buffer, 2));
  EXPECT_EQ(sendFd, 0);
  EXPECT_EQ(sockets[sendFd].port, 2287);
  EXPECT_EQ(sentPort, 2017);
  receivedPort = 2287;
  EXPECT_EQ(port.pollReceive(buffer, sizeof(buffer), &source), 2);
  ASSERT_TRUE(port.sendLocateMulticast(buffer, 2));
  EXPECT_EQ(sendFd, 0);
  EXPECT_EQ(sentPort, 2016);
  port.close();
  EXPECT_TRUE(sockets[0].closed);
  EXPECT_TRUE(sockets[1].closed);
  EXPECT_FALSE(port.isOpen());
  ASSERT_TRUE(port.open(2288, 250));
  EXPECT_EQ(sockets[2].port, 2288);
  EXPECT_EQ(sockets[3].port, 2016);
  EXPECT_TRUE(sockets[3].joined);
}

TEST_F(SupLanEspUdp, DefaultPortUsesOneSocket) {
  Supla::SupLan::EspIdfUdpPort port;
  ASSERT_TRUE(port.open(2016, 250));
  EXPECT_EQ(count, 1);
  EXPECT_TRUE(sockets[0].joined);
  receivedPort = 2016;
  uint8_t buffer[50] = {};
  Supla::SupLan::Endpoint source = {};
  EXPECT_EQ(port.pollReceive(buffer, sizeof(buffer), &source), 2);
}

TEST_F(SupLanEspUdp, DiscoveryBindFailureClosesBothSocketsAndAllowsRetry) {
  Supla::SupLan::EspIdfUdpPort port;
  failBindPort = 2016;
  EXPECT_FALSE(port.open(2287, 250));
  EXPECT_TRUE(sockets[0].closed);
  EXPECT_TRUE(sockets[1].closed);
  EXPECT_FALSE(port.isOpen());
  failBindPort = 0;
  EXPECT_TRUE(port.open(2287, 250));
  EXPECT_TRUE(port.isOpen());
}
