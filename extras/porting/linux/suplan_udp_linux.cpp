// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include "suplan_udp_linux.h"

#include <suplan/suplan_fragment.h>
#include <suplan/suplan_config.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Supla {
namespace SupLan {

namespace {
const uint16_t kDiscoveryPort = 2016;
const char kDiscoveryGroup[] = "239.255.201.6";
const uint8_t kSelfTestPrefix[] = {'S', 'U', 'P', 'T'};
}  // namespace

LinuxUdpPort::LinuxUdpPort()
    : socket_(-1), multicastSocket_(-1), selfTestActive_(false),
      selfTestReceived_(false), capturedLength_(0), captureNextData_(false),
      capturedDataValid_(false), compareNextData_(false),
      compareAfterCapture_(false),
      dataComparisonComplete_(false), dataComparisonMatched_(false) {
  maxDatagramPayload_ = kMaxDatagramPayload;
  localEndpoint_.address = 0;
  localEndpoint_.port = 0;
  selectedAddress_ = htonl(INADDR_ANY);
  automaticInterfaceSelection_ = true;
  memset(interfaceAddresses_, 0, sizeof(interfaceAddresses_));
  interfaceCount_ = 0;
  memset(joinedAddresses_, 0, sizeof(joinedAddresses_));
  joinedCount_ = 0;
  lastInterfaceRefreshMs_ = 0;
  lastMulticastSendCount_ = 0;
  lastMulticastSendAttemptCount_ = 0;
  memset(&openDiagnostics_, 0, sizeof(openDiagnostics_));
  memset(selfTestDatagram_, 0, sizeof(selfTestDatagram_));
  memset(capturedData_, 0, sizeof(capturedData_));
  capturedEndpoint_.address = 0;
  capturedEndpoint_.port = 0;
}

LinuxUdpPort::~LinuxUdpPort() {
  close();
}

bool LinuxUdpPort::open(const char *bindAddress, uint16_t port,
                        size_t maxDatagramPayload) {
  memset(&openDiagnostics_, 0, sizeof(openDiagnostics_));
  selfTestActive_ = false;
  selfTestReceived_ = false;
  if (socket_ >= 0 || multicastSocket_ >= 0 || bindAddress == nullptr ||
      port == 0 ||
      maxDatagramPayload == 0 || maxDatagramPayload > kMaxDatagramPayload) {
    openDiagnostics_.udpBind = kLinuxUdpStageFail;
    return false;
  }
  in_addr localAddress = {};
  if (inet_pton(AF_INET, bindAddress, &localAddress) != 1) {
    openDiagnostics_.udpBind = kLinuxUdpStageFail;
    return false;
  }
  selectedAddress_ = localAddress.s_addr;
  automaticInterfaceSelection_ = localAddress.s_addr == htonl(INADDR_ANY);
  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  multicastSocket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ < 0 || multicastSocket_ < 0) {
    openDiagnostics_.udpBind = kLinuxUdpStageFail;
    openDiagnostics_.multicastGroupJoin = kLinuxUdpStageNotRun;
    close();
    return false;
  }
  int reuse = 1;
  const bool unicastReuse = setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                                       &reuse, sizeof(reuse)) == 0;
  const bool multicastReuse = setsockopt(multicastSocket_, SOL_SOCKET,
                                         SO_REUSEADDR, &reuse,
                                         sizeof(reuse)) == 0;
  sockaddr_in local = {};
  local.sin_family = AF_INET;
  local.sin_port = htons(port);
  local.sin_addr = localAddress;
  const bool unicastBound = unicastReuse &&
      bind(socket_, reinterpret_cast<sockaddr *>(&local), sizeof(local)) == 0;

  sockaddr_in multicastLocal = {};
  multicastLocal.sin_family = AF_INET;
  multicastLocal.sin_port = htons(kDiscoveryPort);
  multicastLocal.sin_addr.s_addr = htonl(INADDR_ANY);
  const bool multicastBound = multicastReuse &&
      bind(multicastSocket_,
           reinterpret_cast<sockaddr *>(&multicastLocal),
           sizeof(multicastLocal)) == 0;
  openDiagnostics_.udpBind = unicastBound && multicastBound
      ? kLinuxUdpStagePass : kLinuxUdpStageFail;

  const int ttl = 1;
  const int loopback = 1;
  const bool ttlSet = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                                 sizeof(ttl)) == 0;
  const bool loopbackSet = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP,
                                      &loopback, sizeof(loopback)) == 0;
  const int flags = fcntl(socket_, F_GETFL, 0);
  const int multicastFlags = fcntl(multicastSocket_, F_GETFL, 0);
  const bool unicastNonBlocking = flags >= 0 &&
      fcntl(socket_, F_SETFL, flags | O_NONBLOCK) == 0;
  const bool multicastNonBlocking = multicastFlags >= 0 &&
      fcntl(multicastSocket_, F_SETFL,
            multicastFlags | O_NONBLOCK) == 0;
  const bool valid = openDiagnostics_.udpBind == kLinuxUdpStagePass && ttlSet &&
      loopbackSet && unicastNonBlocking && multicastNonBlocking;
  if (!valid) {
    close();
    return false;
  }
  (void)refreshMulticastInterfaces();
  maxDatagramPayload_ = maxDatagramPayload;
  localEndpoint_.address = selectedAddress_;
  localEndpoint_.port = port;
  return true;
}

void LinuxUdpPort::close() {
  selfTestActive_ = false;
  if (socket_ >= 0) {
    ::close(socket_);
    socket_ = -1;
  }
  if (multicastSocket_ >= 0) {
    ::close(multicastSocket_);
    multicastSocket_ = -1;
  }
  interfaceCount_ = 0;
  joinedCount_ = 0;
  lastMulticastSendCount_ = 0;
  lastMulticastSendAttemptCount_ = 0;
}

bool LinuxUdpPort::isOpen() const {
  return socket_ >= 0 && multicastSocket_ >= 0 &&
      openDiagnostics_.udpBind == kLinuxUdpStagePass;
}

bool LinuxUdpPort::hasActiveMulticastInterface() const {
  uint32_t addresses[kMaxMulticastInterfaces] = {};
  uint32_t indexes[kMaxMulticastInterfaces] = {};
  size_t count = 0;
  return enumerateMulticastInterfaces(addresses, indexes,
                                      kMaxMulticastInterfaces, &count) &&
      count > 0;
}

bool LinuxUdpPort::enumerateMulticastInterfaces(
    uint32_t *addresses, uint32_t *indexes, size_t capacity,
    size_t *count) const {
  if (addresses == nullptr || indexes == nullptr || count == nullptr ||
      capacity == 0) {
    return false;
  }
  *count = 0;
  ifaddrs *interfaces = nullptr;
  if (getifaddrs(&interfaces) != 0) {
    return false;
  }
  bool complete = true;
  for (ifaddrs *entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
    if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET ||
        entry->ifa_name == nullptr) {
      continue;
    }
    const uint32_t address =
        reinterpret_cast<sockaddr_in *>(entry->ifa_addr)->sin_addr.s_addr;
    if (address == htonl(INADDR_ANY)) {
      continue;
    }
    if (automaticInterfaceSelection_) {
      const unsigned int required = IFF_UP | IFF_RUNNING | IFF_MULTICAST;
      const unsigned int excluded = IFF_LOOPBACK | IFF_POINTOPOINT;
      if ((entry->ifa_flags & required) != required ||
          (entry->ifa_flags & excluded) != 0) {
        continue;
      }
    } else if (address != selectedAddress_) {
      continue;
    }

    const unsigned int index = if_nametoindex(entry->ifa_name);
    if (index == 0) {
      continue;
    }
    bool duplicate = false;
    for (size_t i = 0; i < *count; ++i) {
      if (indexes[i] == index) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }
    if (*count >= capacity) {
      complete = false;
      break;
    }
    addresses[*count] = address;
    indexes[*count] = index;
    ++(*count);
  }
  freeifaddrs(interfaces);
  return complete;
}

bool LinuxUdpPort::refreshMulticastInterfaces() {
  if (multicastSocket_ < 0) {
    return false;
  }
  uint32_t addresses[kMaxMulticastInterfaces] = {};
  uint32_t indexes[kMaxMulticastInterfaces] = {};
  size_t count = 0;
  const bool enumerated = enumerateMulticastInterfaces(
      addresses, indexes, kMaxMulticastInterfaces, &count);
  if (!enumerated) {
    openDiagnostics_.multicastGroupJoin = kLinuxUdpStageFail;
    return false;
  }

  ip_mreq membership = {};
  if (inet_pton(AF_INET, kDiscoveryGroup,
                &membership.imr_multiaddr) != 1) {
    openDiagnostics_.multicastGroupJoin = kLinuxUdpStageFail;
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
    (void)setsockopt(multicastSocket_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
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
    const bool joined = setsockopt(multicastSocket_, IPPROTO_IP,
                                   IP_ADD_MEMBERSHIP, &membership,
                                   sizeof(membership)) == 0 ||
        errno == EADDRINUSE;
    if (joined) {
      joinedAddresses_[joinedCount_++] = addresses[current];
    }
  }

  memcpy(interfaceAddresses_, addresses, count * sizeof(addresses[0]));
  interfaceCount_ = count;
  lastInterfaceRefreshMs_ = nowMs();
  openDiagnostics_.multicastGroupJoin = joinedCount_ > 0
      ? kLinuxUdpStagePass : kLinuxUdpStageFail;
  return true;
}

bool LinuxUdpPort::isLocalAddress(uint32_t address) const {
  if (address == selectedAddress_ && !automaticInterfaceSelection_) {
    return true;
  }
  for (size_t i = 0; i < interfaceCount_; ++i) {
    if (interfaceAddresses_[i] == address) {
      return true;
    }
  }
  return false;
}

bool LinuxUdpPort::sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                               size_t length) {
  if (socket_ < 0 || data == nullptr || length == 0 ||
      length > maxDatagramPayload_) {
    return false;
  }
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = htons(endpoint.port);
  destination.sin_addr.s_addr = endpoint.address;
  const ssize_t sent = sendto(socket_, data, length, 0,
                              reinterpret_cast<sockaddr *>(&destination),
                              sizeof(destination));
  bool capturedThisDatagram = false;
  if (sent == static_cast<ssize_t>(length) && captureNextData_ &&
      length >= 3 && length <= sizeof(capturedData_) &&
      data[0] == kAdaptationFull && data[1] == kVersion &&
      data[2] == kFrameData) {
    memcpy(capturedData_, data, length);
    capturedEndpoint_ = endpoint;
    capturedLength_ = length;
    capturedDataValid_ = true;
    captureNextData_ = false;
    capturedThisDatagram = true;
    if (compareAfterCapture_) {
      compareNextData_ = true;
      dataComparisonComplete_ = false;
      dataComparisonMatched_ = false;
      compareAfterCapture_ = false;
    }
  }
  if (sent == static_cast<ssize_t>(length) && !capturedThisDatagram &&
      compareNextData_ &&
      length >= 3 && data[0] == kAdaptationFull && data[1] == kVersion &&
      data[2] == kFrameData) {
    dataComparisonComplete_ = true;
    dataComparisonMatched_ = capturedDataValid_ &&
        endpoint.address == capturedEndpoint_.address &&
        endpoint.port == capturedEndpoint_.port &&
        length == capturedLength_ &&
        memcmp(data, capturedData_, length) == 0;
    compareNextData_ = false;
  }
  return sent == static_cast<ssize_t>(length);
}

bool LinuxUdpPort::sendLocateMulticast(const uint8_t *data, size_t length) {
  if (socket_ < 0 || data == nullptr || length == 0 ||
      length > maxDatagramPayload_ || !refreshMulticastInterfaces() ||
      interfaceCount_ == 0) {
    return false;
  }
  lastMulticastSendCount_ = 0;
  lastMulticastSendAttemptCount_ = interfaceCount_;
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = htons(kDiscoveryPort);
  if (inet_pton(AF_INET, kDiscoveryGroup, &destination.sin_addr) != 1) {
    return false;
  }
  bool sentOnAnyInterface = false;
  for (size_t i = 0; i < interfaceCount_; ++i) {
    in_addr interfaceAddress = {};
    interfaceAddress.s_addr = interfaceAddresses_[i];
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &interfaceAddress,
                   sizeof(interfaceAddress)) != 0) {
      continue;
    }
    const ssize_t sent = sendto(socket_, data, length, 0,
                                reinterpret_cast<sockaddr *>(&destination),
                                sizeof(destination));
    if (sent == static_cast<ssize_t>(length)) {
      ++lastMulticastSendCount_;
      sentOnAnyInterface = true;
    }
  }
  return sentOnAnyInterface;
}

int LinuxUdpPort::pollReceive(uint8_t *buffer, size_t capacity,
                              Endpoint *endpoint) {
  if (socket_ < 0 || multicastSocket_ < 0 || buffer == nullptr ||
      capacity == 0 ||
      endpoint == nullptr) {
    return -1;
  }
  if (static_cast<uint32_t>(nowMs() - lastInterfaceRefreshMs_) >= 1000) {
    (void)refreshMulticastInterfaces();
  }
  const int sockets[] = {socket_, multicastSocket_};
  for (size_t i = 0; i < sizeof(sockets) / sizeof(sockets[0]); ++i) {
    sockaddr_in source = {};
    socklen_t sourceLength = sizeof(source);
    const ssize_t received = recvfrom(
        sockets[i], buffer, capacity, MSG_DONTWAIT | MSG_TRUNC,
        reinterpret_cast<sockaddr *>(&source), &sourceLength);
    if (received < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      return -1;
    }
    if (static_cast<size_t>(received) > capacity || received > INT32_MAX) {
      return -1;
    }
    if (received == static_cast<ssize_t>(sizeof(selfTestDatagram_)) &&
        memcmp(buffer, kSelfTestPrefix, sizeof(kSelfTestPrefix)) == 0) {
      if (selfTestActive_ &&
          ntohs(source.sin_port) == localEndpoint_.port &&
          memcmp(buffer, selfTestDatagram_, sizeof(selfTestDatagram_)) == 0) {
        selfTestReceived_ = true;
        selfTestActive_ = false;
      }
      continue;
    }
    if (isLocalAddress(source.sin_addr.s_addr) &&
        ntohs(source.sin_port) == localEndpoint_.port) {
      continue;
    }
    endpoint->address = source.sin_addr.s_addr;
    endpoint->port = ntohs(source.sin_port);
    return static_cast<int>(received);
  }
  return 0;
}

size_t LinuxUdpPort::maxDatagramPayload() const {
  return maxDatagramPayload_;
}

uint32_t LinuxUdpPort::nowMs() const {
  timespec now = {};
  if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
    return 0;
  }
  const uint64_t milliseconds = static_cast<uint64_t>(now.tv_sec) * 1000 +
      static_cast<uint64_t>(now.tv_nsec / 1000000);
  return static_cast<uint32_t>(milliseconds);
}

const LinuxUdpOpenDiagnostics &LinuxUdpPort::openDiagnostics() const {
  return openDiagnostics_;
}

size_t LinuxUdpPort::multicastInterfaceCount() const {
  return interfaceCount_;
}

size_t LinuxUdpPort::joinedMulticastInterfaceCount() const {
  return joinedCount_;
}

size_t LinuxUdpPort::lastMulticastSendCount() const {
  return lastMulticastSendCount_;
}

size_t LinuxUdpPort::lastMulticastSendAttemptCount() const {
  return lastMulticastSendAttemptCount_;
}

bool LinuxUdpPort::beginMulticastSelfTest(
    const uint8_t identifier[kSelfTestIdentifierBytes]) {
  if (!isOpen() || identifier == nullptr || selfTestActive_) {
    return false;
  }
  memcpy(selfTestDatagram_, kSelfTestPrefix, sizeof(kSelfTestPrefix));
  memcpy(selfTestDatagram_ + sizeof(kSelfTestPrefix), identifier,
         kSelfTestIdentifierBytes);
  selfTestReceived_ = false;
  selfTestActive_ = true;
  if (!sendLocateMulticast(selfTestDatagram_, sizeof(selfTestDatagram_))) {
    selfTestActive_ = false;
    return false;
  }
  return true;
}

bool LinuxUdpPort::multicastSelfTestReceived() const {
  return selfTestReceived_;
}

void LinuxUdpPort::cancelMulticastSelfTest() {
  selfTestActive_ = false;
}

void LinuxUdpPort::captureNextData(bool compareRetry) {
  capturedLength_ = 0;
  capturedDataValid_ = false;
  captureNextData_ = true;
  compareNextData_ = false;
  compareAfterCapture_ = compareRetry;
  dataComparisonComplete_ = false;
  dataComparisonMatched_ = false;
}

bool LinuxUdpPort::hasCapturedData() const {
  return capturedDataValid_;
}

bool LinuxUdpPort::replayCapturedData() {
  if (!capturedDataValid_ || capturedLength_ == 0) {
    return false;
  }
  return sendUnicast(capturedEndpoint_, capturedData_, capturedLength_);
}

bool LinuxUdpPort::compareNextDataWithCapture() {
  if (!capturedDataValid_ || capturedLength_ == 0) {
    return false;
  }
  dataComparisonComplete_ = false;
  dataComparisonMatched_ = false;
  compareNextData_ = true;
  return true;
}

bool LinuxUdpPort::dataComparisonComplete() const {
  return dataComparisonComplete_;
}

bool LinuxUdpPort::dataComparisonMatched() const {
  return dataComparisonComplete_ && dataComparisonMatched_;
}

}  // namespace SupLan
}  // namespace Supla
