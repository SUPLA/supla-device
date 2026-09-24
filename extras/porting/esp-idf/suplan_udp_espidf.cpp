// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include "suplan_udp_espidf.h"

#include <suplan/suplan_fragment.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <esp_timer.h>
#include <lwip/inet.h>
#include <lwip/ip4_addr.h>
#include <lwip/sockets.h>

namespace Supla {
namespace SupLan {

namespace {
static const uint16_t kDiscoveryPort = 2016;
static const char kDiscoveryGroup[] = "239.255.201.6";
}  // namespace

EspIdfUdpPort::EspIdfUdpPort()
    : socket_(-1), maxDatagramPayload_(kMaxDatagramPayload) {}

EspIdfUdpPort::~EspIdfUdpPort() {
  close();
}

bool EspIdfUdpPort::open(uint16_t port, size_t maxDatagramPayload) {
  if (socket_ >= 0 || port == 0 || maxDatagramPayload == 0 ||
      maxDatagramPayload > kMaxDatagramPayload) {
    return false;
  }
  const int fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) {
    return false;
  }
  int reuse = 1;
  if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                      sizeof(reuse)) != 0) {
    lwip_close(fd);
    return false;
  }
  sockaddr_in local = {};
  local.sin_family = AF_INET;
  local.sin_port = lwip_htons(port);
  local.sin_addr.s_addr = lwip_htonl(INADDR_ANY);
  if (lwip_bind(fd, reinterpret_cast<sockaddr *>(&local), sizeof(local)) != 0) {
    lwip_close(fd);
    return false;
  }

  ip_mreq membership = {};
  if (lwip_inet_pton(AF_INET, kDiscoveryGroup,
                     &membership.imr_multiaddr) != 1) {
    lwip_close(fd);
    return false;
  }
  membership.imr_interface.s_addr = lwip_htonl(INADDR_ANY);
  if (lwip_setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &membership,
                      sizeof(membership)) != 0) {
    lwip_close(fd);
    return false;
  }
  uint8_t ttl = 1;
  if (lwip_setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                      sizeof(ttl)) != 0) {
    lwip_close(fd);
    return false;
  }
  const int flags = lwip_fcntl(fd, F_GETFL, 0);
  if (flags < 0 || lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
    lwip_close(fd);
    return false;
  }
  socket_ = fd;
  maxDatagramPayload_ = maxDatagramPayload;
  return true;
}

void EspIdfUdpPort::close() {
  if (socket_ >= 0) {
    lwip_close(socket_);
    socket_ = -1;
  }
}

bool EspIdfUdpPort::isOpen() const {
  return socket_ >= 0;
}

bool EspIdfUdpPort::sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                                size_t length) {
  if (socket_ < 0 || data == nullptr || length == 0 ||
      length > maxDatagramPayload_) {
    return false;
  }
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = lwip_htons(endpoint.port);
  destination.sin_addr.s_addr = endpoint.address;
  const int sent = lwip_sendto(socket_, data, length, 0,
                               reinterpret_cast<sockaddr *>(&destination),
                               sizeof(destination));
  return sent == static_cast<int>(length);
}

bool EspIdfUdpPort::sendLocateMulticast(const uint8_t *data, size_t length) {
  if (socket_ < 0 || data == nullptr || length == 0 ||
      length > maxDatagramPayload_) {
    return false;
  }
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = lwip_htons(kDiscoveryPort);
  if (lwip_inet_pton(AF_INET, kDiscoveryGroup,
                     &destination.sin_addr) != 1) {
    return false;
  }
  const int sent = lwip_sendto(socket_, data, length, 0,
                               reinterpret_cast<sockaddr *>(&destination),
                               sizeof(destination));
  return sent == static_cast<int>(length);
}

int EspIdfUdpPort::pollReceive(uint8_t *buffer, size_t capacity,
                               Endpoint *endpoint) {
  if (socket_ < 0 || buffer == nullptr || capacity == 0 ||
      endpoint == nullptr) {
    return -1;
  }
  sockaddr_in source = {};
  socklen_t sourceLength = sizeof(source);
  const int received = lwip_recvfrom(socket_, buffer, capacity, MSG_DONTWAIT,
                                     reinterpret_cast<sockaddr *>(&source),
                                     &sourceLength);
  if (received < 0) {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 0 : -1;
  }
  if (static_cast<size_t>(received) > capacity) {
    return -1;
  }
  endpoint->address = source.sin_addr.s_addr;
  endpoint->port = lwip_ntohs(source.sin_port);
  return received;
}

size_t EspIdfUdpPort::maxDatagramPayload() const {
  return maxDatagramPayload_;
}

uint32_t EspIdfUdpPort::nowMs() const {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

}  // namespace SupLan
}  // namespace Supla
