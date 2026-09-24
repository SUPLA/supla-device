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

  ip_mreq membership = {};
  const bool validGroup = inet_pton(AF_INET, kDiscoveryGroup,
                                    &membership.imr_multiaddr) == 1;
  membership.imr_interface = localAddress;
  const bool joined = validGroup &&
      setsockopt(multicastSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                 &membership, sizeof(membership)) == 0;
  openDiagnostics_.multicastGroupJoin = joined
      ? kLinuxUdpStagePass : kLinuxUdpStageFail;

  const int ttl = 1;
  const int loopback = 1;
  const bool ttlSet = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
                                 sizeof(ttl)) == 0;
  const bool interfaceSet = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
                                       &localAddress,
                                       sizeof(localAddress)) == 0;
  const bool loopbackSet = setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP,
                                      &loopback, sizeof(loopback)) == 0;
  const int flags = fcntl(socket_, F_GETFL, 0);
  const int multicastFlags = fcntl(multicastSocket_, F_GETFL, 0);
  const bool unicastNonBlocking = flags >= 0 &&
      fcntl(socket_, F_SETFL, flags | O_NONBLOCK) == 0;
  const bool multicastNonBlocking = multicastFlags >= 0 &&
      fcntl(multicastSocket_, F_SETFL,
            multicastFlags | O_NONBLOCK) == 0;
  const bool valid = openDiagnostics_.udpBind == kLinuxUdpStagePass &&
      openDiagnostics_.multicastGroupJoin == kLinuxUdpStagePass && ttlSet &&
      interfaceSet && loopbackSet && unicastNonBlocking &&
      multicastNonBlocking;
  if (!valid) {
    close();
    return false;
  }
  maxDatagramPayload_ = maxDatagramPayload;
  localEndpoint_.address = localAddress.s_addr;
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
}

bool LinuxUdpPort::isOpen() const {
  return socket_ >= 0 && multicastSocket_ >= 0 &&
      openDiagnostics_.udpBind == kLinuxUdpStagePass &&
      openDiagnostics_.multicastGroupJoin == kLinuxUdpStagePass;
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
      length > maxDatagramPayload_) {
    return false;
  }
  sockaddr_in destination = {};
  destination.sin_family = AF_INET;
  destination.sin_port = htons(kDiscoveryPort);
  if (inet_pton(AF_INET, kDiscoveryGroup, &destination.sin_addr) != 1) {
    return false;
  }
  const ssize_t sent = sendto(socket_, data, length, 0,
                              reinterpret_cast<sockaddr *>(&destination),
                              sizeof(destination));
  return sent == static_cast<ssize_t>(length);
}

int LinuxUdpPort::pollReceive(uint8_t *buffer, size_t capacity,
                              Endpoint *endpoint) {
  if (socket_ < 0 || multicastSocket_ < 0 || buffer == nullptr ||
      capacity == 0 ||
      endpoint == nullptr) {
    return -1;
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
          source.sin_addr.s_addr == localEndpoint_.address &&
          ntohs(source.sin_port) == localEndpoint_.port &&
          memcmp(buffer, selfTestDatagram_, sizeof(selfTestDatagram_)) == 0) {
        selfTestReceived_ = true;
        selfTestActive_ = false;
      }
      continue;
    }
    if (source.sin_addr.s_addr == localEndpoint_.address &&
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
