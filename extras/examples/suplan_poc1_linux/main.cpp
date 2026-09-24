// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 AC SOFTWARE SP. Z O.O.

#include <SuplaDevice.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/control/virtual_relay.h>
#include <supla/protocol/suplan_protocol.h>
#include <suplan/suplan_runtime.h>
#include <suplan/suplan_wire.h>
#include <suplan_poc1_profile.h>

#include <suplan_crypto_openssl.h>
#include <suplan_udp_linux.h>

#include <errno.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <string>
#include <yaml-cpp/yaml.h>

// linux_log.c expects the sd4linux application settings.
int logLevel = LOG_INFO;
int runAsDaemon = 0;

namespace {

struct HarnessState {
  const char *role;
  Supla::Control::VirtualRelay *relay;
  Supla::AtChannel *actionChannel;
  uint32_t stateEvents;
  uint32_t actionEvents;
  uint32_t acknowledgements;
};

struct MulticastSelfTestState {
  bool active;
  bool transmitSucceeded;
  uint32_t deadlineMs;
  Supla::SupLan::Diagnostics baseline;
};

uint32_t getLe32(const uint8_t *input) {
  return static_cast<uint32_t>(input[0]) |
      (static_cast<uint32_t>(input[1]) << 8) |
      (static_cast<uint32_t>(input[2]) << 16) |
      (static_cast<uint32_t>(input[3]) << 24);
}

void putLe32(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

void onApplicationEvent(
    void *context, Supla::Protocol::SupLanApplicationEvent event,
    uint8_t, const Supla::SupLan::ResourceId &resource, uint32_t messageType,
    const uint8_t *payload, size_t payloadLength) {
  HarnessState *state = static_cast<HarnessState *>(context);
  if (event == Supla::Protocol::kSupLanRemoteState && payload != nullptr) {
    ++state->stateEvents;
    std::cout << "EVENT " << state->role << " STATE resource=" << resource.id
              << " value=" << (payloadLength >= 14
                    ? static_cast<unsigned>(payload[6]) : 0)
              << " payload_bytes=" << payloadLength
              << " count=" << state->stateEvents << std::endl;
  } else if (event == Supla::Protocol::kSupLanRemoteAction &&
             payload != nullptr && payloadLength == sizeof(TDS_ActionTrigger)) {
    ++state->actionEvents;
    std::cout << "EVENT " << state->role << " ACTION resource=" << resource.id
              << " action=" << getLe32(payload + 1)
              << " count=" << state->actionEvents << std::endl;
  } else if (event == Supla::Protocol::kSupLanOperationAck &&
             payload != nullptr && payloadLength == 5) {
    ++state->acknowledgements;
    std::cout << "EVENT " << state->role << " ACK resource=" << resource.id
              << " sequence=" << getLe32(payload)
              << " result=" << static_cast<unsigned>(payload[4]) << std::endl;
  } else if (event == Supla::Protocol::kSupLanReadInterest) {
    std::cout << "EVENT " << state->role << " READ resource=" << resource.id
              << " event_only=" << messageType << std::endl;
  }
}

void printHelp() {
  std::cout << "Commands: help, show-status, show-resources, "
               "read <resource-id>, multicast-self-test, "
               "control <resource-id> <value>, emit-action <resource-id> "
               "<action-id>, set-resource-value <resource-id> <value>, "
               "drop-next-tx <ACK|DATA|FRAGMENT|SESSION_ACCEPT>, "
               "drop-next-rx <ACK|DATA|FRAGMENT|SESSION_ACCEPT>, "
               "corrupt-next-tx-tag, corrupt-next-tx-mac, "
               "corrupt-next-session-mac-tx, force-max-datagram <bytes>, "
               "drop-fragment-number <n>, send-extended <resource-id> <bytes>, "
               "capture-next-data, compare-next-data, show-data-comparison, "
               "replay-captured-data, "
               "inject-oversized-fragment <peer>, "
               "pause-session-init-retries, resume-session-init-retries, "
               "forget-session <peer>, clear-endpoint <peer>, "
               "flood-invalid-locate|flood-invalid-session|flood-invalid-data "
               "<count>, show-pools, show-counters, reset-test-counters, quit\n"
               "Static profile: A owns relay CHANNEL:50001; B owns event-only "
               "CHANNEL:50002. Keys are test fixtures only.\n";
}

const char *stageStatus(Supla::SupLan::LinuxUdpStageStatus status) {
  switch (status) {
    case Supla::SupLan::kLinuxUdpStagePass:
      return "PASS";
    case Supla::SupLan::kLinuxUdpStageFail:
      return "FAIL";
    case Supla::SupLan::kLinuxUdpStageNotRun:
    default:
      return "NOT RUN";
  }
}

std::string interfaceNameForAddress(const std::string &address) {
  in_addr target = {};
  if (inet_pton(AF_INET, address.c_str(), &target) != 1) {
    return "unknown";
  }
  ifaddrs *interfaces = nullptr;
  if (getifaddrs(&interfaces) != 0) {
    return "unknown";
  }
  std::string result = "unknown";
  for (ifaddrs *entry = interfaces; entry != nullptr; entry = entry->ifa_next) {
    if (entry->ifa_addr != nullptr && entry->ifa_addr->sa_family == AF_INET &&
        reinterpret_cast<sockaddr_in *>(entry->ifa_addr)->sin_addr.s_addr ==
            target.s_addr) {
      result = entry->ifa_name;
      break;
    }
  }
  freeifaddrs(interfaces);
  return result;
}

bool peerDiscovered(const Supla::SupLan::PeerTable &peers) {
  for (uint8_t i = 0; i < peers.size(); ++i) {
    const Supla::SupLan::PeerRecord *peer = peers.get(i);
    if (peer != nullptr &&
        peer->endpointState != Supla::SupLan::kPeerEndpointNone) {
      return true;
    }
  }
  return false;
}

bool unicastAttemptedSince(const Supla::SupLan::Diagnostics &current,
                           const Supla::SupLan::Diagnostics &baseline) {
  return current.sessionInitTx > baseline.sessionInitTx ||
      current.sessionInitRx > baseline.sessionInitRx ||
      current.sessionAcceptTx > baseline.sessionAcceptTx ||
      current.sessionAcceptRx > baseline.sessionAcceptRx ||
      current.sessionRejectTx > baseline.sessionRejectTx ||
      current.sessionRejectRx > baseline.sessionRejectRx;
}

void printMulticastSelfTestResult(
    const std::string &interfaceName, const std::string &bindAddress,
    const Supla::SupLan::LinuxUdpPort &datagrams,
    const Supla::SupLan::Runtime &runtime,
    const Supla::SupLan::PeerTable &peers,
    const MulticastSelfTestState &test, bool receiveSucceeded) {
  const Supla::SupLan::LinuxUdpOpenDiagnostics &open =
      datagrams.openDiagnostics();
  std::cout << "UDP bind                 " << stageStatus(open.udpBind) << '\n'
            << "Multicast group join     "
            << stageStatus(open.multicastGroupJoin) << '\n'
            << "Multicast transmit       "
            << (test.transmitSucceeded ? "PASS" : "FAIL") << '\n'
            << "Local multicast receive  "
            << (receiveSucceeded ? "PASS" : "FAIL") << '\n'
            << "SupLAN peer discovered   "
            << (peerDiscovered(peers) ? "YES" : "NO") << '\n';

  const Supla::SupLan::Diagnostics &current = runtime.diagnostics();
  if (current.sessionEstablished > 0) {
    std::cout << "Unicast communication    PASS\n";
  } else if (unicastAttemptedSince(current, test.baseline)) {
    std::cout << "Unicast communication    FAIL\n";
  } else {
    std::cout << "Unicast communication    NOT OBSERVED\n";
  }

  if (!receiveSucceeded) {
    std::cout << "\nSupLAN multicast self-test failed.\n"
              << "Interface: " << interfaceName << '\n'
              << "Bind address: " << bindAddress << '\n'
              << "Discovery destination: 239.255.201.6:2016\n\n";
    if (test.transmitSucceeded) {
      std::cout << "Multicast traffic sent through this interface is not "
                   "reaching the local\n"
                   "SupLAN receiver. Check host firewall, interface "
                   "selection and multicast\n"
                   "configuration.\n";
    } else {
      std::cout << "The diagnostic multicast datagram could not be sent. "
                   "Check UDP socket setup and interface configuration.\n";
    }
  }
  if (!peerDiscovered(peers)) {
    std::cout << "SupLAN peer discovered = NO is inconclusive; no peer may "
                 "be present.\n";
  }
  std::cout << std::flush;
}

void printCounters(const Supla::SupLan::Runtime &runtime) {
  const Supla::SupLan::Diagnostics &d = runtime.diagnostics();
  std::cout << "COUNTERS locate_tx=" << d.locateTx
            << " locate_rx=" << d.locateRx
            << " locate_reply_tx=" << d.locateReplyTx
            << " locate_reply_rx=" << d.locateReplyRx
            << " session_init_tx=" << d.sessionInitTx
            << " session_init_rx=" << d.sessionInitRx
            << " session_accept_tx=" << d.sessionAcceptTx
            << " session_accept_rx=" << d.sessionAcceptRx
            << " session_established=" << d.sessionEstablished
            << " session_replaced=" << d.sessionReplaced
            << " session_reject_tx=" << d.sessionRejectTx
            << " session_reject_rx=" << d.sessionRejectRx
            << " session_unknown_drop=" << d.sessionUnknownDrop
            << " data_tx=" << d.dataTx << " data_rx=" << d.dataRx
            << " auth_fail=" << d.dataAuthFail
            << " replay_drop=" << d.dataReplayDrop
            << " duplicate=" << d.dataDuplicate << " ack_tx=" << d.ackTx
            << " ack_rx=" << d.ackRx << " retry_tx=" << d.retryTx
            << " controls=" << d.controlDispatched
            << " control_duplicates=" << d.controlDuplicateSuppressed
            << " reads=" << d.readDispatched
            << " states_tx=" << d.stateNotificationTx
            << " states_rx=" << d.stateNotificationRx
            << " actions_tx=" << d.actionTx << " actions_rx=" << d.actionRx
            << " fragments_tx=" << d.fragmentTx
            << " fragments_rx=" << d.fragmentRx
            << " reassembly_start=" << d.reassemblyStarted
            << " reassembly_done=" << d.reassemblyCompleted
            << " reassembly_expired=" << d.reassemblyExpired
            << " reassembly_rejected=" << d.reassemblyRejected
            << " acl_reject=" << d.aclReject
            << " resource_not_found=" << d.resourceNotFound
            << " invalid_locate=" << d.invalidLocateDrop
            << " invalid_session=" << d.invalidSessionDrop
            << " invalid_data=" << d.invalidDataDrop
            << " deferred_queue_overflow=" << d.deferredQueueOverflow
            << " pool_reject=" << d.poolReject << std::endl;
}

void printPools(const Supla::SupLan::Runtime &runtime) {
  const Supla::SupLan::PoolDiagnostics p = runtime.poolDiagnostics();
#define PRINT_POOL(name) \
  std::cout << " " #name "=" << p.name.used << "/" << p.name.maximum \
            << "(high=" << p.name.highWater << ")"
  std::cout << "POOLS";
  PRINT_POOL(peers);
  PRINT_POOL(aclEntries);
  PRINT_POOL(sessions);
  PRINT_POOL(pending);
  PRINT_POOL(locates);
  PRINT_POOL(interests);
  PRINT_POOL(reassembly);
  PRINT_POOL(retries);
  PRINT_POOL(deferredEvents);
  std::cout << " workspace_bytes=" << p.workspaceBytes << std::endl;
#undef PRINT_POOL
}

uint16_t parseValue(const std::string &text, uint16_t fallback) {
  char *end = nullptr;
  const uint64_t value = strtoull(text.c_str(), &end, 0);
  return end != text.c_str() && *end == '\0' && value <= UINT16_MAX
      ? static_cast<uint16_t>(value) : fallback;
}

bool parsePort(const std::string &text, uint16_t *port) {
  if (port == nullptr) {
    return false;
  }
  char *end = nullptr;
  const uint64_t value = strtoull(text.c_str(), &end, 0);
  if (end == text.c_str() || *end != '\0' || value == 0 ||
      value > UINT16_MAX) {
    return false;
  }
  *port = static_cast<uint16_t>(value);
  return true;
}

bool loadUnicastPort(const std::string &configPath, uint16_t *port) {
  if (configPath.empty() || port == nullptr) {
    return port != nullptr;
  }
  try {
    const YAML::Node config = YAML::LoadFile(configPath);
    const YAML::Node suplan = config["suplan"];
    if (!suplan) {
      return true;
    }
    if (!suplan.IsMap()) {
      return false;
    }
    const YAML::Node configuredPort = suplan["unicast_port"];
    if (!configuredPort) {
      return true;
    }
    const unsigned int value = configuredPort.as<unsigned int>();
    if (value == 0 || value > UINT16_MAX) {
      return false;
    }
    *port = static_cast<uint16_t>(value);
    return true;
  } catch (const YAML::Exception &) {
    return false;
  }
}

void setHook(uint8_t *counter, const std::string &command,
             const std::string &frameKind) {
  if (counter == nullptr) {
    std::cout << "ERROR unknown frame kind" << std::endl;
  } else {
    *counter = 1;
    std::cout << "OK hook=" << command << " frame=" << frameKind << std::endl;
  }
}

void handleCommand(const std::string &line, Supla::SupLan::Runtime *runtime,
                   Supla::Protocol::SupLan *protocol, HarnessState *state,
                   Supla::SupLan::LinuxUdpPort *datagrams,
                   Supla::SupLan::RandomPort *random,
                   Supla::SupLan::PeerTable *peers,
                   const std::string &interfaceName,
                   const std::string &bindAddress,
                   MulticastSelfTestState *selfTest, bool *running) {
  std::istringstream input(line);
  std::string command;
  input >> command;
  if (command.empty()) return;
  if (command == "help") {
    printHelp();
  } else if (command == "quit") {
    *running = false;
  } else if (command == "show-status") {
    printCounters(*runtime);
    printPools(*runtime);
  } else if (command == "multicast-self-test") {
    if (selfTest->active) {
      std::cout << "ERROR multicast self-test already running" << std::endl;
      return;
    }
    uint8_t identifier[Supla::SupLan::LinuxUdpPort::
                           kSelfTestIdentifierBytes] = {};
    if (random == nullptr || !random->fillRandom(identifier,
                                                 sizeof(identifier))) {
      selfTest->transmitSucceeded = false;
      selfTest->baseline = runtime->diagnostics();
      printMulticastSelfTestResult(interfaceName, bindAddress, *datagrams,
                                   *runtime, *peers, *selfTest, false);
      return;
    }
    selfTest->baseline = runtime->diagnostics();
    selfTest->transmitSucceeded = datagrams->beginMulticastSelfTest(
        identifier);
    if (!selfTest->transmitSucceeded) {
      printMulticastSelfTestResult(interfaceName, bindAddress, *datagrams,
                                   *runtime, *peers, *selfTest, false);
      return;
    }
    selfTest->active = true;
    selfTest->deadlineMs = datagrams->nowMs() + 1500;
    std::cout << "OK multicast self-test started interface="
              << interfaceName << " discovery=239.255.201.6:2016"
              << std::endl;
  } else if (command == "show-counters") {
    printCounters(*runtime);
  } else if (command == "show-pools") {
    printPools(*runtime);
  } else if (command == "show-resources") {
    std::cout << "RESOURCES 50001=relay:" << (state->relay != nullptr &&
                    state->relay->isOn() ? 1 : 0)
              << " control_dispatched="
              << runtime->diagnostics().controlDispatched
              << " 50002=event-only actions=" << state->actionEvents
              << std::endl;
  } else if (command == "read") {
    uint32_t id = 0;
    input >> id;
    const uint8_t peer = id == Supla::SupLan::Poc1::kActionResourceId ? 1 : 0;
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    std::cout << (runtime->requestRead(peer, resource) ? "OK read queued"
                                                      : "ERROR read rejected")
              << std::endl;
  } else if (command == "control") {
    uint32_t id = 0;
    unsigned value = 0;
    input >> id >> value;
    uint8_t payload[17] = {};
    putLe32(payload, 1);
    payload[4] = 0xFF;
    putLe32(payload + 5, 0);
    payload[9] = static_cast<uint8_t>(value);
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, id};
    std::cout << (value <= 255 && runtime->sendControl(0, resource, payload,
                                                       sizeof(payload))
                     ? "OK control queued" : "ERROR control rejected")
              << std::endl;
  } else if (command == "emit-action") {
    uint32_t id = 0;
    uint32_t actionId = 0;
    input >> id >> actionId;
    if (state->actionChannel != nullptr &&
        id == Supla::SupLan::Poc1::kActionResourceId) {
      // Use the existing ProtocolLayer callback used by AtChannel::sendUpdate.
      protocol->sendActionTrigger(0, actionId);
      std::cout << "OK action callback invoked" << std::endl;
    } else {
      std::cout << "ERROR action resource is not local" << std::endl;
    }
  } else if (command == "set-resource-value") {
    uint32_t id = 0;
    unsigned value = 0;
    input >> id >> value;
    if (state->relay == nullptr ||
        id != Supla::SupLan::Poc1::kRelayResourceId ||
        value > 1) {
      std::cout << "ERROR expected local resource 50001 and value 0 or 1"
                << std::endl;
    } else {
      if (value != 0) state->relay->turnOn();
      else state->relay->turnOff();
      Supla::Channel *channel = Supla::Channel::GetByChannelNumber(0);
      if (channel != nullptr) channel->sendUpdate();
      std::cout << "OK local SUPLA Channel updated" << std::endl;
    }
  } else if (command == "send-extended") {
    uint32_t id = 0;
    unsigned length = 0;
    input >> id >> length;
    if (state->relay == nullptr ||
        id != Supla::SupLan::Poc1::kRelayResourceId ||
        length == 0 || length > 750) {
      std::cout << "ERROR expected local resource 50001 and bytes 1..750"
                << std::endl;
    } else {
      uint8_t prefix[6] = {0xFF, 100, 0, 0, 0, 0};
      putLe32(prefix + 2, length);
      uint8_t payload[750];
      for (unsigned i = 0; i < length; ++i) {
        payload[i] = static_cast<uint8_t>((i * 37U + 11U) & 0xFFU);
      }
      const Supla::SupLan::ResourceId resource = {
          Supla::SupLan::kResourceTypeChannel, id};
      std::cout << (runtime->publishStateParts(
                        0, resource,
                        Supla::SupLan::
                            kSuplaCallDeviceChannelExtendedValueChanged,
                        prefix, sizeof(prefix), payload, length)
                        ? "OK extended state queued"
                        : "ERROR extended state rejected")
                << std::endl;
    }
  } else if (command == "capture-next-data") {
    std::string option;
    input >> option;
    const bool compareRetry = option == "compare-retry";
    datagrams->captureNextData(compareRetry);
    std::cout << "OK capture-next-data armed"
              << (compareRetry ? " with retry comparison" : "")
              << std::endl;
  } else if (command == "compare-next-data") {
    std::cout << (datagrams->compareNextDataWithCapture()
                      ? "OK data comparison armed"
                      : "ERROR no captured DATA")
              << std::endl;
  } else if (command == "show-data-comparison") {
    if (!datagrams->dataComparisonComplete()) {
      std::cout << "DATA_COMPARE=PENDING" << std::endl;
    } else {
      std::cout << "DATA_COMPARE="
                << (datagrams->dataComparisonMatched() ? "PASS" : "FAIL")
                << std::endl;
    }
  } else if (command == "replay-captured-data") {
    std::cout << (datagrams->replayCapturedData()
                      ? "OK captured DATA replayed"
                      : "ERROR no captured DATA")
              << std::endl;
  } else if (command == "inject-oversized-fragment") {
    unsigned peerIndex = 0;
    input >> peerIndex;
    const Supla::SupLan::PeerRecord *peer = peerIndex <= UINT8_MAX
        ? peers->get(static_cast<uint8_t>(peerIndex)) : nullptr;
    uint8_t fragment[Supla::SupLan::kFragmentHeaderSize + 1] = {};
    fragment[0] = Supla::SupLan::kAdaptationFragment;
    Supla::SupLan::putUint32(fragment + 1, 0x5355504CU);
    Supla::SupLan::putUint16(
        fragment + 5,
        static_cast<uint16_t>(SUPLAN_RX_MAX_REASSEMBLED_FRAME + 1));
    Supla::SupLan::putUint16(fragment + 7, 0);
    fragment[Supla::SupLan::kFragmentHeaderSize] = 0;
    const bool sent = peer != nullptr &&
        peer->endpointState == Supla::SupLan::kPeerEndpointAuthenticated &&
        datagrams->sendUnicast(peer->endpoint, fragment, sizeof(fragment));
    std::cout << (sent ? "OK oversized fragment injected"
                       : "ERROR peer has no authenticated endpoint")
              << std::endl;
  } else if (command == "force-max-datagram") {
    unsigned value = 0;
    input >> value;
    if (value >= 64 && value <= Supla::SupLan::kMaxDatagramPayload) {
      runtime->testHooks()->maxDatagramPayload = value;
      std::cout << "OK max-datagram=" << value << std::endl;
    } else {
      std::cout << "ERROR max datagram outside supported range" << std::endl;
    }
  } else if (command == "drop-next-tx" || command == "drop-next-rx") {
    std::string kind;
    input >> kind;
    Supla::SupLan::RuntimeTestHooks *hooks = runtime->testHooks();
    uint8_t *counter = nullptr;
    const bool tx = command == "drop-next-tx";
    if (kind == "ACK" || kind == "ack")
      counter = tx ? &hooks->dropNextAckTx : &hooks->dropNextAckRx;
    else if (kind == "DATA" || kind == "data")
      counter = tx ? &hooks->dropNextDataTx : &hooks->dropNextDataRx;
    else if (kind == "FRAGMENT" || kind == "fragment")
      counter = tx ? &hooks->dropNextFragmentTx : &hooks->dropNextFragmentRx;
    else if (kind == "SESSION_ACCEPT" || kind == "session_accept")
      counter = tx ? &hooks->dropNextSessionAcceptTx
                   : &hooks->dropNextSessionAcceptRx;
    setHook(counter, command, kind);
  } else if (command == "corrupt-next-tx-tag") {
    runtime->testHooks()->corruptNextDataTagTx = 1;
    std::cout << "OK hook=" << command << std::endl;
  } else if (command == "corrupt-next-tx-mac") {
    runtime->testHooks()->corruptNextMacTx = 1;
    std::cout << "OK hook=" << command << std::endl;
  } else if (command == "corrupt-next-session-mac-tx") {
    runtime->testHooks()->corruptNextSessionMacTx = 1;
    std::cout << "OK hook=" << command << std::endl;
  } else if (command == "pause-session-init-retries") {
    runtime->testHooks()->pauseSessionInitRetries = 1;
    std::cout << "OK session-init retries paused" << std::endl;
  } else if (command == "resume-session-init-retries") {
    runtime->testHooks()->pauseSessionInitRetries = 0;
    std::cout << "OK session-init retries resumed" << std::endl;
  } else if (command == "drop-fragment-number") {
    unsigned value = 0;
    input >> value;
    runtime->testHooks()->dropFragmentNumber = static_cast<uint16_t>(value);
    std::cout << "OK hook=" << command << " value=" << value << std::endl;
  } else if (command == "forget-session" || command == "clear-endpoint") {
    unsigned peer = 0;
    input >> peer;
    if (peer > 1) {
      std::cout << "ERROR profile peers are 0 and 1" << std::endl;
    } else if (command == "forget-session") {
      std::cout << (runtime->forgetSession(peer) ? "OK session forgotten"
                                                : "OK no active session")
                << std::endl;
    } else {
      runtime->clearEndpoint(peer);
      std::cout << "OK endpoint cleared" << std::endl;
    }
  } else if (command == "flood-invalid-locate" ||
             command == "flood-invalid-session" ||
             command == "flood-invalid-data") {
    unsigned count = 0;
    input >> count;
    Supla::SupLan::TestFloodKind kind = Supla::SupLan::kFloodInvalidData;
    if (command == "flood-invalid-locate") {
      kind = Supla::SupLan::kFloodInvalidLocate;
    } else if (command == "flood-invalid-session") {
      kind = Supla::SupLan::kFloodInvalidSession;
    }
    std::cout << (count <= 1000 && runtime->startFlood(kind, 0, count)
                     ? "OK flood started" : "ERROR flood rejected")
              << std::endl;
  } else if (command == "reset-test-counters") {
    runtime->resetDiagnostics();
    std::cout << "OK counters reset" << std::endl;
  } else {
    std::cout << "ERROR unknown command: " << command << std::endl;
  }
}

bool parseArgs(int argc, char **argv, std::string *role,
               std::string *bindAddress, std::string *configPath,
               uint16_t *cliPort, bool *hasCliPort, size_t *maxDatagram) {
  if (role == nullptr || bindAddress == nullptr || configPath == nullptr ||
      cliPort == nullptr || hasCliPort == nullptr || maxDatagram == nullptr) {
    return false;
  }
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--help") {
      std::cout << "Usage: suplan-poc1-linux --role A|B --bind IPv4 "
                   "[--config file.yaml] [--suplan-port 2016] "
                   "[--max-datagram 250]\n";
      printHelp();
      return false;
    }
    if (i + 1 >= argc) {
      return false;
    }
    if (arg == "--role") {
      *role = argv[++i];
    } else if (arg == "--bind") {
      *bindAddress = argv[++i];
    } else if (arg == "--config") {
      *configPath = argv[++i];
    } else if (arg == "--suplan-port" || arg == "--port") {
      if (!parsePort(argv[++i], cliPort)) {
        return false;
      }
      *hasCliPort = true;
    } else if (arg == "--max-datagram") {
      *maxDatagram = parseValue(argv[++i], *maxDatagram);
    } else {
      return false;
    }
  }
  return (*role == "A" || *role == "B") && !bindAddress->empty() &&
      *maxDatagram >= 64 &&
      *maxDatagram <= Supla::SupLan::kMaxDatagramPayload;
}

}  // namespace

int main(int argc, char **argv) {
  std::string role;
  std::string bindAddress;
  std::string configPath;
  uint16_t cliPort = 0;
  bool hasCliPort = false;
  uint16_t port = 2016;
  size_t maxDatagram = Supla::SupLan::kMaxDatagramPayload;
  if (!parseArgs(argc, argv, &role, &bindAddress, &configPath, &cliPort,
                 &hasCliPort, &maxDatagram)) {
    if (argc == 1) {
      std::cerr << "--role and --bind are required; use --help" << std::endl;
    }
    return argc == 2 && std::string(argv[1]) == "--help" ? 0 : 2;
  }
  if (!loadUnicastPort(configPath, &port)) {
    std::cerr << "invalid suplan.unicast_port in " << configPath << std::endl;
    return 2;
  }
  if (hasCliPort) {
    port = cliPort;
  }

  const std::string interfaceName = interfaceNameForAddress(bindAddress);
  Supla::SupLan::OpenSslCryptoPort crypto;
  Supla::SupLan::OpenSslRandomPort random;
  Supla::SupLan::PeerTable peers;
  uint8_t primaryPeer = 0;
  uint8_t actionPeer = 0;
  const bool nodeA = role == "A";
  if (!Supla::SupLan::Poc1::configurePeerTable(
          &crypto, &peers, nodeA, &primaryPeer, &actionPeer)) {
    std::cerr << "failed to load static PoC profile" << std::endl;
    return 2;
  }

  Supla::SupLan::LinuxUdpPort datagrams;
  if (!datagrams.open(bindAddress.c_str(), port, maxDatagram)) {
    std::cerr << "failed to open IPv4 UDP " << bindAddress << ':' << port
              << "; bind to an interface address with multicast support"
              << std::endl;
    const Supla::SupLan::LinuxUdpOpenDiagnostics &open =
        datagrams.openDiagnostics();
    std::cerr << "UDP bind                 " << stageStatus(open.udpBind)
              << '\n'
              << "Multicast group join     "
              << stageStatus(open.multicastGroupJoin) << '\n'
              << "Multicast transmit       NOT RUN\n"
              << "Local multicast receive  NOT RUN\n"
              << "SupLAN peer discovered   NO\n"
              << "Unicast communication    NOT OBSERVED\n"
              << "Interface: " << interfaceName << '\n'
              << "Discovery destination: 239.255.201.6:2016\n";
    return 2;
  }

  HarnessState state = {role.c_str(), nullptr, nullptr, 0, 0, 0};
  if (nodeA) {
    state.relay = new Supla::Control::VirtualRelay();
    state.relay->setDefaultStateOff();
    state.relay->onInit();
  } else {
    state.actionChannel = new Supla::AtChannel();
  }
  const Supla::Protocol::SupLanResourceMapping mapping = nodeA
      ? Supla::Protocol::SupLanResourceMapping{
            Supla::SupLan::Poc1::kRelayResourceId, 0, primaryPeer, false}
      : Supla::Protocol::SupLanResourceMapping{
            Supla::SupLan::Poc1::kActionResourceId, 0, actionPeer, true};
  Supla::Protocol::SupLan protocol(nullptr, &peers, &mapping, 1,
                                   onApplicationEvent, &state);
  Supla::SupLan::Runtime runtime(
      &crypto, &random, &datagrams, &protocol, &peers,
      Supla::SupLan::Poc1::localNodeAddress(nodeA),
      Supla::SupLan::kMinimumSuplaProtoVersion);
  protocol.attachRuntime(&runtime);
  if (!protocol.verifyConfig()) {
    std::cerr << "invalid SupLAN ProtocolLayer mapping" << std::endl;
    return 2;
  }

  std::cout << "READY role=" << role << " bind=" << bindAddress << ':' << port
            << " discovery=239.255.201.6:2016 peers=2 fixture=static"
            << std::endl;
  printHelp();

  bool running = true;
  MulticastSelfTestState selfTest = {};
  char line[512] = {};
  size_t lineLength = 0;
  uint32_t lastControlDispatch = 0;
  while (running) {
    pollfd descriptor = {STDIN_FILENO, POLLIN, 0};
    const int ready = poll(&descriptor, 1, 10);
    if (ready > 0 && (descriptor.revents & POLLIN) != 0) {
      char bytes[256];
      const ssize_t count = read(STDIN_FILENO, bytes, sizeof(bytes));
      if (count > 0) {
        for (ssize_t i = 0; i < count; ++i) {
          if (bytes[i] == '\n') {
            line[lineLength] = '\0';
            handleCommand(std::string(line), &runtime, &protocol, &state,
                          &datagrams, &random, &peers, interfaceName,
                          bindAddress, &selfTest, &running);
            lineLength = 0;
          } else if (lineLength + 1 < sizeof(line)) {
            line[lineLength++] = bytes[i];
          } else {
            lineLength = 0;
            std::cout << "ERROR command line too long" << std::endl;
          }
        }
      }
    } else if (ready < 0 && errno != EINTR) {
      std::cerr << "stdin poll failed" << std::endl;
      return 2;
    }
    runtime.iterate();
    if (selfTest.active) {
      const bool received = datagrams.multicastSelfTestReceived();
      if (received || static_cast<int32_t>(datagrams.nowMs() -
                                           selfTest.deadlineMs) >= 0) {
        selfTest.active = false;
        datagrams.cancelMulticastSelfTest();
        printMulticastSelfTestResult(interfaceName, bindAddress, datagrams,
                                     runtime, peers, selfTest, received);
      }
    }
    const uint32_t dispatched = runtime.diagnostics().controlDispatched;
    if (dispatched != lastControlDispatch) {
      lastControlDispatch = dispatched;
      std::cout << "EVENT " << role << " CONTROL resource=50001 value="
                << (state.relay != nullptr && state.relay->isOn() ? 1 : 0)
                << " count=" << dispatched << std::endl;
    }
  }
  return 0;
}
