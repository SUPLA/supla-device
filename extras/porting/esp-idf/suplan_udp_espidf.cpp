// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include "suplan_udp_espidf.h"

#include <suplan/suplan_fragment.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <esp_netif.h>
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
    : socket_(-1), maxDatagramPayload_(kMaxDatagramPayload),
      interfaceCount_(0), joinedCount_(0), lastInterfaceRefreshMs_(0) {
  memset(interfaceAddresses_, 0, sizeof(interfaceAddresses_));
  memset(joinedAddresses_, 0, sizeof(joinedAddresses_));
}

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
  (void)refreshMulticastInterfaces();
  return true;
}

void EspIdfUdpPort::close() {
  if (socket_ >= 0) {
    lwip_close(socket_);
    socket_ = -1;
  }
  interfaceCount_ = 0;
  joinedCount_ = 0;
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
      length > maxDatagramPayload_ || !refreshMulticastInterfaces() ||
      interfaceCount_ == 0) {
    return false;
  }
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = lwip_htons(kDiscoveryPort);
  if (lwip_inet_pton(AF_INET, kDiscoveryGroup,
                     &destination.sin_addr) != 1) {
    return false;
  }
  bool sentOnAnyInterface = false;
  for (size_t i = 0; i < interfaceCount_; ++i) {
    in_addr interfaceAddress = {};
    interfaceAddress.s_addr = interfaceAddresses_[i];
    if (lwip_setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                        &interfaceAddress, sizeof(interfaceAddress)) != 0) {
      continue;
    }
    const int sent = lwip_sendto(socket_, data, length, 0,
                                 reinterpret_cast<sockaddr *>(&destination),
                                 sizeof(destination));
    sentOnAnyInterface = sentOnAnyInterface ||
        sent == static_cast<int>(length);
  }
  return sentOnAnyInterface;
}

int EspIdfUdpPort::pollReceive(uint8_t *buffer, size_t capacity,
                               Endpoint *endpoint) {
  if (socket_ < 0 || buffer == nullptr || capacity == 0 ||
      endpoint == nullptr) {
    return -1;
  }
  if (static_cast<uint32_t>(nowMs() - lastInterfaceRefreshMs_) >= 1000) {
    (void)refreshMulticastInterfaces();
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

size_t EspIdfUdpPort::multicastInterfaceCount() const {
  return interfaceCount_;
}

size_t EspIdfUdpPort::joinedMulticastInterfaceCount() const {
  return joinedCount_;
}

bool EspIdfUdpPort::refreshMulticastInterfaces() {
  if (socket_ < 0) {
    return false;
  }
  uint32_t addresses[kMaxMulticastInterfaces] = {};
  size_t count = 0;
  for (esp_netif_t *netif = esp_netif_next_unsafe(nullptr); netif != nullptr;
       netif = esp_netif_next_unsafe(netif)) {
    if (!esp_netif_is_netif_up(netif)) {
      continue;
    }
    esp_netif_ip_info_t ipInfo = {};
    if (esp_netif_get_ip_info(netif, &ipInfo) != ESP_OK ||
        ipInfo.ip.addr == 0) {
      continue;
    }
    bool duplicate = false;
    for (size_t i = 0; i < count; ++i) {
      if (addresses[i] == ipInfo.ip.addr) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    if (count >= kMaxMulticastInterfaces) {
      return false;
    }
    addresses[count++] = ipInfo.ip.addr;
  }

  ip_mreq membership = {};
  if (lwip_inet_pton(AF_INET, kDiscoveryGroup,
                     &membership.imr_multiaddr) != 1) {
    return false;
  }
  for (size_t old = 0; old < joinedCount_;) {
    bool stillAvailable = false;
    for (size_t current = 0; current < count; ++current) {
      if (joinedAddresses_[old] == addresses[current]) {
        stillAvailable = true;
        break;
      }
    }
    if (stillAvailable) {
      ++old;
      continue;
    }
    membership.imr_interface.s_addr = joinedAddresses_[old];
    (void)lwip_setsockopt(socket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                          &membership, sizeof(membership));
    for (size_t move = old + 1; move < joinedCount_; ++move) {
      joinedAddresses_[move - 1] = joinedAddresses_[move];
    }
    --joinedCount_;
  }

  for (size_t current = 0; current < count; ++current) {
    bool alreadyJoined = false;
    for (size_t joined = 0; joined < joinedCount_; ++joined) {
      if (joinedAddresses_[joined] == addresses[current]) {
        alreadyJoined = true;
        break;
      }
    }
    if (alreadyJoined || joinedCount_ >= kMaxMulticastInterfaces) {
      continue;
    }
    membership.imr_interface.s_addr = addresses[current];
    if (lwip_setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        &membership, sizeof(membership)) == 0 ||
        errno == EADDRINUSE) {
      joinedAddresses_[joinedCount_++] = addresses[current];
    }
  }

  memcpy(interfaceAddresses_, addresses, count * sizeof(addresses[0]));
  interfaceCount_ = count;
  lastInterfaceRefreshMs_ = nowMs();
  return true;
}

}  // namespace SupLan
}  // namespace Supla
