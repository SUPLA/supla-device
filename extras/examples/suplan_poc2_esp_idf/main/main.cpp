// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <SuplaDevice.h>
#include <esp_idf_web_server.h>
#include <esp_idf_wifi.h>
#include <nvs_config.h>
#include <spiffs_storage.h>
#include <supla/debug/debug_log_tcp_server.h>
#include <supla/device/status_led.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/virtual_relay.h>
#include <supla/network/network.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/time.h>
#include <suplan/suplan_runtime.h>

#include <suplan_crypto_mbedtls.h>
#include <suplan_udp_espidf.h>
#include <suplan_poc1_profile.h>

#include <new>

namespace {

using Supla::SupLan::Diagnostics;
using Supla::SupLan::Runtime;
using Protocol = Supla::Protocol::SupLan;
using ResourceMapping = Supla::Protocol::SupLanResourceMapping;

#if CONFIG_SUPLAN_POC2_ROLE_A
constexpr bool kNodeA = true;
constexpr char kRole = 'A';
#else
constexpr bool kNodeA = false;
constexpr char kRole = 'B';
#endif

constexpr uint16_t kUnicastPort = CONFIG_SUPLAN_POC2_UNICAST_PORT;

static Supla::SupLan::EspIdfUdpPort datagrams;

class EspIdfTransportLifecycle
    : public Supla::Protocol::SupLanTransportLifecycle {
 public:
  bool networkReady(uint32_t) override {
    return Supla::Network::IsReady();
  }

  bool isOpen() const override {
    return datagrams.isOpen();
  }

  bool open() override {
    if (!datagrams.open(kUnicastPort,
                        Supla::SupLan::kMaxDatagramPayload)) {
      return false;
    }
    if (datagrams.multicastInterfaceCount() == 0 ||
        datagrams.joinedMulticastInterfaceCount() == 0) {
      datagrams.close();
      return false;
    }
    return true;
  }

  void close() override {
    datagrams.close();
  }
};

static Supla::SupLan::MbedTlsCryptoPort crypto;
static Supla::SupLan::PeerTable peers;
static EspIdfTransportLifecycle transport;
static uint8_t primaryPeer = 0;
static uint8_t actionPeer = 1;
static Supla::Control::VirtualRelay* relay = nullptr;
static Supla::AtChannel* actionChannel = nullptr;
using Supla::SupLan::Poc1::kRelayResourceId;
using Supla::SupLan::Poc1::kActionResourceId;
static constexpr ResourceMapping kMappingA = {kRelayResourceId, 0, 0, false};
static constexpr ResourceMapping kMappingB = {kActionResourceId, 0, 1, true};
alignas(Protocol) static uint8_t protocolStorage[sizeof(Protocol)];
alignas(Runtime) static uint8_t runtimeStorage[sizeof(Runtime)];
static Protocol* protocol = nullptr;
static Runtime* runtime = nullptr;
static char inputLine[128];
static size_t inputLength = 0;

void putLe32(uint8_t* output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

uint32_t getLe32(const uint8_t* input) {
  return static_cast<uint32_t>(input[0]) |
      (static_cast<uint32_t>(input[1]) << 8) |
      (static_cast<uint32_t>(input[2]) << 16) |
      (static_cast<uint32_t>(input[3]) << 24);
}

void applicationEvent(
    void*, Supla::Protocol::SupLanApplicationEvent event, uint8_t peerIndex,
    const Supla::SupLan::ResourceId& resource, uint32_t messageType,
    const uint8_t* payload, size_t payloadLength) {
  if (event == Supla::Protocol::kSupLanRemoteState) {
    printf("EVENT STATE peer=%u resource=%" PRIu32 " bytes=%u\n",
           peerIndex, resource.id, static_cast<unsigned>(payloadLength));
  } else if (event == Supla::Protocol::kSupLanRemoteAction) {
    printf("EVENT ACTION peer=%u resource=%" PRIu32 " action=%" PRIu32
           "\n", peerIndex, resource.id,
           payload != nullptr && payloadLength >= 5 ? getLe32(payload + 1)
                                                     : messageType);
  } else if (event == Supla::Protocol::kSupLanOperationAck &&
             payload != nullptr && payloadLength == 5) {
    printf("EVENT ACK peer=%u resource=%" PRIu32 " sequence=%" PRIu32
           " result=%u\n", peerIndex, resource.id, getLe32(payload),
           payload[4]);
  } else if (event == Supla::Protocol::kSupLanReadInterest) {
    printf("EVENT READ peer=%u resource=%" PRIu32 " event_only=%" PRIu32
           "\n", peerIndex, resource.id, messageType);
  }
}

bool setupSupLan() {
  if (!Supla::SupLan::Poc1::configurePeerTable(
          &peers, kNodeA, &primaryPeer, &actionPeer)) {
    return false;
  }
  const ResourceMapping* mapping = kNodeA ? &kMappingA : &kMappingB;
  protocol = new (protocolStorage) Protocol(
      &SuplaDevice, &peers, mapping, 1, applicationEvent, nullptr);
  runtime = new (runtimeStorage) Runtime(
      &crypto, &datagrams, protocol, &peers,
      Supla::SupLan::Poc1::localNodeAddress(kNodeA),
      Supla::SupLan::kMinimumSuplaProtoVersion);
  protocol->attachRuntime(runtime);
  protocol->attachTransportLifecycle(&transport);
  return protocol->verifyConfig();
}

void showStatus() {
  const bool srpcReady = SuplaDevice.getSrpcLayer() != nullptr &&
      SuplaDevice.getSrpcLayer()->isRegisteredAndReady();
  const bool srpcConnected = SuplaDevice.getSrpcLayer() != nullptr &&
      SuplaDevice.getSrpcLayer()->isConnected();
  const Supla::SupLan::PoolDiagnostics pools = runtime->poolDiagnostics();
  printf("STATUS network_ready=%u srpc_connected=%u srpc_registered=%u "
         "device_status=%d "
         "suplan_enabled=%u "
         "transport=%s sessions=%u role=%c port=%u\n",
         Supla::Network::IsReady() ? 1U : 0U, srpcConnected ? 1U : 0U,
         srpcReady ? 1U : 0U,
         static_cast<int>(SuplaDevice.getCurrentStatus()),
         protocol->isEnabled() ? 1U : 0U,
         protocol->isTransportOpen() ? "open" : "closed",
         pools.sessions.used, kRole, kUnicastPort);
}

void showCounters() {
  const Diagnostics& d = runtime->diagnostics();
  printf("COUNTERS locate_tx=%" PRIu32 " locate_rx=%" PRIu32
         " locate_reply_tx=%" PRIu32 " locate_reply_rx=%" PRIu32
         " session_init_tx=%" PRIu32 " session_init_rx=%" PRIu32
         " session_accept_tx=%" PRIu32 " session_accept_rx=%" PRIu32
         " session_established=%" PRIu32 " session_replaced=%" PRIu32
         " data_tx=%" PRIu32 " data_rx=%" PRIu32
         " auth_fail=%" PRIu32 " replay_drop=%" PRIu32
         " duplicate=%" PRIu32 " ack_tx=%" PRIu32 " ack_rx=%" PRIu32
         " retry_tx=%" PRIu32 " controls=%" PRIu32
         " control_duplicates=%" PRIu32 " reads=%" PRIu32
         " states_tx=%" PRIu32 " states_rx=%" PRIu32
         " actions_tx=%" PRIu32 " actions_rx=%" PRIu32
         " fragments_tx=%" PRIu32 " fragments_rx=%" PRIu32
         " reassembly_start=%" PRIu32 " reassembly_done=%" PRIu32
         " reassembly_expired=%" PRIu32 " reassembly_rejected=%" PRIu32
         " acl_reject=%" PRIu32 " invalid_locate=%" PRIu32
         " invalid_session=%" PRIu32 " invalid_data=%" PRIu32
         " deferred_queue_overflow=%" PRIu32 " pool_reject=%" PRIu32
         "\n",
         d.locateTx, d.locateRx, d.locateReplyTx, d.locateReplyRx,
         d.sessionInitTx, d.sessionInitRx, d.sessionAcceptTx,
         d.sessionAcceptRx, d.sessionEstablished, d.sessionReplaced,
         d.dataTx, d.dataRx, d.dataAuthFail, d.dataReplayDrop,
         d.dataDuplicate, d.ackTx, d.ackRx, d.retryTx, d.controlDispatched,
         d.controlDuplicateSuppressed, d.readDispatched,
         d.stateNotificationTx, d.stateNotificationRx, d.actionTx, d.actionRx,
         d.fragmentTx, d.fragmentRx, d.reassemblyStarted,
         d.reassemblyCompleted, d.reassemblyExpired, d.reassemblyRejected,
         d.aclReject, d.invalidLocateDrop, d.invalidSessionDrop,
         d.invalidDataDrop, d.deferredQueueOverflow, d.poolReject);
}

void showPools() {
  const Supla::SupLan::PoolDiagnostics p = runtime->poolDiagnostics();
  printf("POOLS peers=%u/%u(high=%u) aclEntries=%u/%u(high=%u) "
         "sessions=%u/%u(high=%u) pending=%u/%u(high=%u) "
         "locates=%u/%u(high=%u) interests=%u/%u(high=%u) "
         "reassembly=%u/%u(high=%u) retries=%u/%u(high=%u) "
         "deferredEvents=%u/%u(high=%u) workspace_bytes=%" PRIu32 "\n",
         p.peers.used, p.peers.maximum, p.peers.highWater,
         p.aclEntries.used, p.aclEntries.maximum, p.aclEntries.highWater,
         p.sessions.used, p.sessions.maximum, p.sessions.highWater,
         p.pending.used, p.pending.maximum, p.pending.highWater,
         p.locates.used, p.locates.maximum, p.locates.highWater,
         p.interests.used, p.interests.maximum, p.interests.highWater,
         p.reassembly.used, p.reassembly.maximum, p.reassembly.highWater,
         p.retries.used, p.retries.maximum, p.retries.highWater,
         p.deferredEvents.used, p.deferredEvents.maximum,
         p.deferredEvents.highWater, p.workspaceBytes);
}

void handleCommand(const char* line) {
  if (strcmp(line, "help") == 0) {
    printf("Commands: show-status, show-resources, show-counters, show-pools, "
           "read <resource>, control <resource> <0|1>, "
           "set-resource-value <resource> <0|1>, emit-action <resource> "
           "<action-id>, forget-session <peer>, clear-endpoint <peer>\n");
  } else if (strcmp(line, "show-status") == 0) {
    showStatus();
  } else if (strcmp(line, "show-counters") == 0) {
    showCounters();
  } else if (strcmp(line, "show-pools") == 0) {
    showPools();
  } else if (strcmp(line, "show-resources") == 0) {
    if (relay != nullptr) {
      printf("RESOURCES 50001=relay:%u channel=0\n",
             relay->isOn() ? 1U : 0U);
    } else {
      printf("RESOURCES 50002=action-trigger channel=0\n");
    }
  } else {
    unsigned resource = 0;
    unsigned value = 0;
    unsigned actionId = 0;
    unsigned peer = 0;
    if (sscanf(line, "read %u", &resource) == 1) {
      const uint8_t peerIndex =
          resource == Supla::SupLan::Poc1::kActionResourceId
          ? actionPeer : primaryPeer;
      const Supla::SupLan::ResourceId id = {
          Supla::SupLan::kResourceTypeChannel, resource};
      printf(runtime->requestRead(peerIndex, id) ? "OK read queued\n"
                                                 : "ERROR read rejected\n");
    } else if (sscanf(line, "control %u %u", &resource, &value) == 2) {
      uint8_t payload[17] = {};
      putLe32(payload, 1);
      payload[4] = Supla::SupLan::kChannelNumberUnresolved;
      putLe32(payload + 5, 0);
      payload[9] = static_cast<uint8_t>(value);
      const Supla::SupLan::ResourceId id = {
          Supla::SupLan::kResourceTypeChannel, resource};
      printf(value <= 1 && runtime->sendControl(primaryPeer, id, payload,
                                                sizeof(payload))
                 ? "OK control queued\n" : "ERROR control rejected\n");
    } else if (sscanf(line, "set-resource-value %u %u", &resource,
                      &value) == 2) {
      if (relay == nullptr ||
          resource != Supla::SupLan::Poc1::kRelayResourceId || value > 1) {
        printf("ERROR expected local relay 50001 and value 0 or 1\n");
      } else {
        if (value != 0) {
          relay->turnOn();
        } else {
          relay->turnOff();
        }
        Supla::Channel* channel = Supla::Channel::GetByChannelNumber(0);
        if (channel != nullptr) {
          channel->sendUpdate();
        }
        printf("OK local SUPLA Channel updated\n");
      }
    } else if (sscanf(line, "emit-action %u %u", &resource, &actionId) == 2) {
      if (actionChannel == nullptr ||
          resource != Supla::SupLan::Poc1::kActionResourceId ||
          actionId >= 32) {
        printf("ERROR expected local action resource 50002 and id 0..31\n");
      } else {
        actionChannel->pushAction(1U << actionId);
        actionChannel->sendUpdate();
        printf("OK action queued through local Channel\n");
      }
    } else if (sscanf(line, "forget-session %u", &peer) == 1 &&
               peer < peers.size()) {
      printf(runtime->forgetSession(static_cast<uint8_t>(peer))
                 ? "OK session forgotten\n" : "ERROR session not found\n");
    } else if (sscanf(line, "clear-endpoint %u", &peer) == 1 &&
               peer < peers.size()) {
      runtime->clearEndpoint(static_cast<uint8_t>(peer));
      printf("OK endpoint cleared\n");
    } else {
      printf("ERROR unknown command; type help\n");
    }
  }
}

void pollConsole() {
  uint8_t input[64];
  const ssize_t amount = read(STDIN_FILENO, input, sizeof(input));
  if (amount <= 0) {
    if (amount < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
        errno != EINTR) {
      printf("ERROR console read failed errno=%d\n", errno);
    }
    return;
  }
  for (ssize_t i = 0; i < amount; ++i) {
    if (input[i] == '\n' || input[i] == '\r') {
      inputLine[inputLength] = '\0';
      if (inputLength != 0) {
        handleCommand(inputLine);
      }
      inputLength = 0;
    } else if (inputLength + 1 < sizeof(inputLine)) {
      inputLine[inputLength++] = static_cast<char>(input[i]);
    } else {
      inputLength = 0;
      printf("ERROR command line too long\n");
    }
  }
}

void cppMain(void*) {
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;
  new Supla::Device::StatusLed(18, false);
  new Supla::SpiffsStorage(512);
  new Supla::NvsConfig;
  new Supla::EspIdfWebServer;
  esp_log_level_set("SUPLA", ESP_LOG_DEBUG);

  Supla::EspIdfWifi wifi;
  if (kNodeA) {
    relay = new Supla::Control::VirtualRelay();
    relay->setDefaultStateOff();
  } else {
    auto* actionTrigger = new Supla::Control::ActionTrigger();
    actionChannel = static_cast<Supla::AtChannel*>(
        actionTrigger->getChannel());
  }

  if (!setupSupLan()) {
    printf("SupLAN PoC2 fixture initialization failed\n");
    vTaskDelete(nullptr);
    return;
  }

  printf("SupLAN PoC2 role=%c fixture=static port=%u\n", kRole,
         kUnicastPort);
  printf("Normal Wi-Fi/SRPC lifecycle; type help for test commands.\n");
  if (!SuplaDevice.begin()) {
    printf("SuplaDevice.begin reported initialization failure\n");
  }

#if SUPLA_INSECURE_DEBUG_INTERFACE
  Supla::Debug::DebugLogTcpServer debugLogServer(7778);
  debugLogServer.begin();
#endif

  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags >= 0) {
    (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  }
  const TickType_t delayTicks = pdMS_TO_TICKS(5) > 0 ? pdMS_TO_TICKS(5) : 1;
  while (true) {
    SuplaDevice.iterate();
#if SUPLA_INSECURE_DEBUG_INTERFACE
    debugLogServer.iterate();
#endif
    pollConsole();
    vTaskDelay(delayTicks);
  }
}

}  // namespace

extern "C" void app_main() {
  xTaskCreate(cppMain, "suplan_poc2", 8192, nullptr, 5, nullptr);
}
