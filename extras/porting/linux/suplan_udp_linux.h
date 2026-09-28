// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#ifndef EXTRAS_PORTING_LINUX_SUPLAN_UDP_LINUX_H_
#define EXTRAS_PORTING_LINUX_SUPLAN_UDP_LINUX_H_

#include <suplan/suplan_ports.h>
#include <suplan/suplan_fragment.h>

namespace Supla {
namespace SupLan {

enum LinuxUdpStageStatus : uint8_t {
  kLinuxUdpStageNotRun = 0,
  kLinuxUdpStagePass = 1,
  kLinuxUdpStageFail = 2,
};

struct LinuxUdpOpenDiagnostics {
  LinuxUdpStageStatus udpBind;
  LinuxUdpStageStatus multicastGroupJoin;
};

class LinuxUdpPort : public DatagramPort {
 public:
  static const size_t kSelfTestIdentifierBytes = 12;
  static const size_t kMaxMulticastInterfaces = 16;

  LinuxUdpPort();
  ~LinuxUdpPort() override;

  bool open(const char *bindAddress, uint16_t port,
            size_t maxDatagramPayload);
  void close();
  bool isOpen() const;
  bool sendUnicast(const Endpoint &endpoint, const uint8_t *data,
                   size_t length) override;
  bool sendLocateMulticast(const uint8_t *data, size_t length) override;
  int pollReceive(uint8_t *buffer, size_t capacity,
                  Endpoint *endpoint) override;
  size_t maxDatagramPayload() const override;
  uint32_t nowMs() const override;
  const LinuxUdpOpenDiagnostics &openDiagnostics() const;
  size_t multicastInterfaceCount() const;
  size_t joinedMulticastInterfaceCount() const;
  size_t lastMulticastSendCount() const;
  size_t lastMulticastSendAttemptCount() const;
  bool beginMulticastSelfTest(
      const uint8_t identifier[kSelfTestIdentifierBytes]);
  bool multicastSelfTestReceived() const;
  void cancelMulticastSelfTest();
  void captureNextData(bool compareRetry = false);
  bool hasCapturedData() const;
  bool replayCapturedData();
  bool compareNextDataWithCapture();
  bool dataComparisonComplete() const;
  bool dataComparisonMatched() const;

 private:
  bool refreshMulticastInterfaces();
  bool enumerateMulticastInterfaces(uint32_t *addresses,
                                    uint32_t *indexes, size_t capacity,
                                    size_t *count) const;
  bool isLocalAddress(uint32_t address) const;

  int socket_;
  int multicastSocket_;
  size_t maxDatagramPayload_;
  Endpoint localEndpoint_;
  LinuxUdpOpenDiagnostics openDiagnostics_;
  uint32_t selectedAddress_;
  bool automaticInterfaceSelection_;
  uint32_t interfaceAddresses_[kMaxMulticastInterfaces];
  size_t interfaceCount_;
  uint32_t joinedAddresses_[kMaxMulticastInterfaces];
  size_t joinedCount_;
  uint32_t lastInterfaceRefreshMs_;
  size_t lastMulticastSendCount_;
  size_t lastMulticastSendAttemptCount_;
  uint8_t selfTestDatagram_[16];
  bool selfTestActive_;
  bool selfTestReceived_;
  uint8_t capturedData_[kMaxDatagramPayload];
  Endpoint capturedEndpoint_;
  size_t capturedLength_;
  bool captureNextData_;
  bool capturedDataValid_;
  bool compareNextData_;
  bool compareAfterCapture_;
  bool dataComparisonComplete_;
  bool dataComparisonMatched_;
};

}  // namespace SupLan
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLAN_UDP_LINUX_H_
