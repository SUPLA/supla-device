// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <SuplaDevice.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/control/virtual_relay.h>
#include <supla/protocol/suplan_protocol.h>
#include <suplan/suplan_runtime.h>
#include <suplan_poc1_profile.h>

#include <suplan_crypto_mbedtls.h>
#include <suplan_udp_espidf.h>

#include <new>

namespace {

using Supla::SupLan::Diagnostics;
using Supla::SupLan::PoolDiagnostics;
using Supla::SupLan::Runtime;
using Protocol = Supla::Protocol::SupLan;
using ResourceMapping = Supla::Protocol::SupLanResourceMapping;

#if CONFIG_SUPLAN_POC1_ROLE_A
static const bool kNodeA = true;
static const char kRole[] = "A";
#else
static const bool kNodeA = false;
static const char kRole[] = "B";
#endif

static EventGroupHandle_t wifiEvents;
static const EventBits_t kWifiConnected = BIT0;
static Supla::SupLan::MbedTlsCryptoPort crypto;
static Supla::SupLan::EspIdfRandomPort randomPort;
static Supla::SupLan::EspIdfUdpPort datagrams;
static Supla::SupLan::PeerTable peers;
static uint8_t primaryPeer = 0;
static uint8_t actionPeer = 1;
static Supla::Control::VirtualRelay *relay = nullptr;
static Supla::AtChannel *actionChannel = nullptr;
static uint32_t remoteStateEvents = 0;
static uint32_t remoteActionEvents = 0;
static uint32_t ackEvents = 0;

static constexpr ResourceMapping mappingA() {
  return {Supla::SupLan::Poc1::kRelayResourceId, 0, 0, false};
}

static constexpr ResourceMapping mappingB() {
  return {Supla::SupLan::Poc1::kActionResourceId, 0, 1, true};
}

static const ResourceMapping kMappingA = mappingA();
static const ResourceMapping kMappingB = mappingB();
static const Supla::Protocol::SupLanResourceMapping *localMapping() {
  return kNodeA ? &kMappingA : &kMappingB;
}

alignas(Protocol) static uint8_t protocolStorage[sizeof(Protocol)];
alignas(Runtime) static uint8_t runtimeStorage[sizeof(Runtime)];
static Supla::Protocol::SupLan *protocol = nullptr;
static Runtime *runtime = nullptr;
static uint8_t extendedPayload[750];

static uint32_t getLe32(const uint8_t *input) {
  return static_cast<uint32_t>(input[0]) |
      (static_cast<uint32_t>(input[1]) << 8) |
      (static_cast<uint32_t>(input[2]) << 16) |
      (static_cast<uint32_t>(input[3]) << 24);
}

static void putLe32(uint8_t *output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

static void applicationEvent(
    void *, Supla::Protocol::SupLanApplicationEvent event, uint8_t,
    const Supla::SupLan::ResourceId &resource, uint32_t messageType,
    const uint8_t *payload, size_t length) {
  if (event == Supla::Protocol::kSupLanRemoteState && payload != nullptr) {
    ++remoteStateEvents;
    printf("EVENT STATE resource=%" PRIu32 " value=%u bytes=%u"
           " count=%" PRIu32 "\n",
           resource.id, length >= 14 ? payload[6] : 0,
           static_cast<unsigned>(length), remoteStateEvents);
  } else if (event == Supla::Protocol::kSupLanRemoteAction &&
             payload != nullptr && length == sizeof(TDS_ActionTrigger)) {
    ++remoteActionEvents;
    printf("EVENT ACTION resource=%" PRIu32 " action=%" PRIu32
           " count=%" PRIu32 "\n",
           resource.id, getLe32(payload + 1), remoteActionEvents);
  } else if (event == Supla::Protocol::kSupLanOperationAck &&
             payload != nullptr && length == 5) {
    ++ackEvents;
    printf("EVENT ACK resource=%" PRIu32 " sequence=%" PRIu32 " result=%u\n",
           resource.id, getLe32(payload), payload[4]);
  } else if (event == Supla::Protocol::kSupLanReadInterest) {
    printf("EVENT READ resource=%" PRIu32 " event_only=%" PRIu32 "\n",
           resource.id, messageType);
  }
}

static void printCounters() {
  const Diagnostics &d = runtime->diagnostics();
  printf("COUNTERS locate_tx=%" PRIu32 " locate_rx=%" PRIu32
         " locate_reply_tx=%" PRIu32 " locate_reply_rx=%" PRIu32
         " session_init_tx=%" PRIu32 " session_init_rx=%" PRIu32
         " session_accept_tx=%" PRIu32 " session_accept_rx=%" PRIu32
         " session_established=%" PRIu32 " session_replaced=%" PRIu32
         " session_reject_tx=%" PRIu32 " session_reject_rx=%" PRIu32
         " session_unknown_drop=%" PRIu32 " data_tx=%" PRIu32
         " data_rx=%" PRIu32 " auth_fail=%" PRIu32
         " replay_drop=%" PRIu32 " duplicate=%" PRIu32
         " ack_tx=%" PRIu32 " ack_rx=%" PRIu32 " retry_tx=%" PRIu32
         " controls=%" PRIu32 " control_duplicates=%" PRIu32
         " reads=%" PRIu32 " states_tx=%" PRIu32 " states_rx=%" PRIu32
         " actions_tx=%" PRIu32 " actions_rx=%" PRIu32
         " fragments_tx=%" PRIu32 " fragments_rx=%" PRIu32
         " reassembly_start=%" PRIu32 " reassembly_done=%" PRIu32
         " reassembly_expired=%" PRIu32 " reassembly_rejected=%" PRIu32
         " acl_reject=%" PRIu32 " resource_not_found=%" PRIu32
         " invalid_locate=%" PRIu32 " invalid_session=%" PRIu32
         " invalid_data=%" PRIu32 " deferred_queue_overflow=%" PRIu32
         " pool_reject=%" PRIu32 "\n",
         d.locateTx, d.locateRx, d.locateReplyTx, d.locateReplyRx,
         d.sessionInitTx, d.sessionInitRx, d.sessionAcceptTx,
         d.sessionAcceptRx, d.sessionEstablished, d.sessionReplaced,
         d.sessionRejectTx, d.sessionRejectRx, d.sessionUnknownDrop,
         d.dataTx, d.dataRx, d.dataAuthFail, d.dataReplayDrop,
         d.dataDuplicate, d.ackTx, d.ackRx, d.retryTx, d.controlDispatched,
         d.controlDuplicateSuppressed, d.readDispatched,
         d.stateNotificationTx, d.stateNotificationRx, d.actionTx,
         d.actionRx, d.fragmentTx, d.fragmentRx, d.reassemblyStarted,
         d.reassemblyCompleted, d.reassemblyExpired, d.reassemblyRejected,
         d.aclReject, d.resourceNotFound, d.invalidLocateDrop,
         d.invalidSessionDrop, d.invalidDataDrop, d.deferredQueueOverflow,
         d.poolReject);
}

static void printPools() {
  const PoolDiagnostics p = runtime->poolDiagnostics();
  printf("POOLS peers=%u/%u(high=%u) aclEntries=%u/%u(high=%u)"
         " sessions=%u/%u(high=%u) pending=%u/%u(high=%u)"
         " locates=%u/%u(high=%u) interests=%u/%u(high=%u)"
         " reassembly=%u/%u(high=%u) retries=%u/%u(high=%u)"
         " deferredEvents=%u/%u(high=%u) workspace_bytes=%" PRIu32 "\n",
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

static uint64_t parseNumber(const char *value, uint64_t fallback) {
  if (value == nullptr || *value == '\0') return fallback;
  char *end = nullptr;
  errno = 0;
  const uint64_t parsed = static_cast<uint64_t>(strtoull(value, &end, 0));
  return errno != ERANGE && end != value && *end == '\0' ? parsed : fallback;
}

static uint8_t peerForResource(uint32_t id) {
  return id == Supla::SupLan::Poc1::kActionResourceId ? actionPeer
                                                       : primaryPeer;
}

static void command(char *line, bool *running) {
  char *save = nullptr;
  char *name = strtok_r(line, " \t\r\n", &save);
  if (name == nullptr) return;
  char *arg1 = strtok_r(nullptr, " \t\r\n", &save);
  char *arg2 = strtok_r(nullptr, " \t\r\n", &save);
  const uint64_t first = parseNumber(arg1, UINT64_MAX);
  const uint64_t second = parseNumber(arg2, UINT64_MAX);
  if (strcmp(name, "help") == 0) {
    printf("Commands: show-status, show-counters, show-pools, show-resources,"
           " read <resource-id>, control <resource-id> <0|1>,"
           " emit-action 50002 <action-id>, set-resource-value 50001 <0|1>,"
           " send-extended 50001 <bytes>, drop-next-tx|drop-next-rx"
           " <ACK|DATA|FRAGMENT|SESSION_ACCEPT>, corrupt-next-tx-tag,"
           " corrupt-next-tx-mac, corrupt-next-session-mac-tx,"
           " force-max-datagram <bytes>, drop-fragment-number <n>,"
           " forget-session|clear-endpoint <peer>, flood-invalid-locate|"
           "flood-invalid-session|flood-invalid-data <count>,"
           " reset-test-counters, quit\n"
           "Fixture only: A relay CHANNEL:50001, B event CHANNEL:50002.\n");
  } else if (strcmp(name, "quit") == 0) {
    *running = false;
  } else if (strcmp(name, "show-status") == 0) {
    printCounters();
    printPools();
  } else if (strcmp(name, "show-counters") == 0) {
    printCounters();
  } else if (strcmp(name, "show-pools") == 0) {
    printPools();
  } else if (strcmp(name, "show-resources") == 0) {
    printf("RESOURCES 50001=relay:%u control_dispatched=%" PRIu32
           " 50002=event-only actions=%" PRIu32 "\n",
           relay != nullptr && relay->isOn() ? 1 : 0,
           runtime->diagnostics().controlDispatched, remoteActionEvents);
  } else if (strcmp(name, "read") == 0 && first != UINT64_MAX &&
             first <= UINT32_MAX) {
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, static_cast<uint32_t>(first)};
    printf(runtime->requestRead(peerForResource(resource.id), resource)
               ? "OK read queued\n" : "ERROR read rejected\n");
  } else if (strcmp(name, "control") == 0 && first != UINT64_MAX &&
             first <= UINT32_MAX && second <= 1) {
    uint8_t payload[17] = {};
    putLe32(payload, 1);
    payload[4] = 0xFF;
    payload[9] = static_cast<uint8_t>(second);
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, static_cast<uint32_t>(first)};
    printf(runtime->sendControl(primaryPeer, resource, payload,
                                sizeof(payload))
               ? "OK control queued\n" : "ERROR control rejected\n");
  } else if (strcmp(name, "emit-action") == 0 && actionChannel != nullptr &&
             first == Supla::SupLan::Poc1::kActionResourceId &&
             second != UINT64_MAX && second <= UINT32_MAX) {
    protocol->sendActionTrigger(0, static_cast<uint32_t>(second));
    printf("OK action callback invoked\n");
  } else if (strcmp(name, "set-resource-value") == 0 && relay != nullptr &&
             first == Supla::SupLan::Poc1::kRelayResourceId && second <= 1) {
    if (second != 0) {
      relay->turnOn();
    } else {
      relay->turnOff();
    }
    Supla::Channel *channel = Supla::Channel::GetByChannelNumber(0);
    if (channel != nullptr) channel->sendUpdate();
    printf("OK local SUPLA Channel updated\n");
  } else if (strcmp(name, "send-extended") == 0 && relay != nullptr &&
             first == Supla::SupLan::Poc1::kRelayResourceId &&
             second > 0 && second <= sizeof(extendedPayload)) {
    uint8_t prefix[6] = {0xFF, 100, 0, 0, 0, 0};
    putLe32(prefix + 2, static_cast<uint32_t>(second));
    for (unsigned i = 0; i < second; ++i) {
      extendedPayload[i] = static_cast<uint8_t>((i * 37U + 11U) & 0xFFU);
    }
    const Supla::SupLan::ResourceId resource = {
        Supla::SupLan::kResourceTypeChannel, static_cast<uint32_t>(first)};
    printf(runtime->publishStateParts(
               primaryPeer, resource,
               Supla::SupLan::kSuplaCallDeviceChannelExtendedValueChanged,
               prefix, sizeof(prefix), extendedPayload, second)
               ? "OK extended state queued\n"
               : "ERROR extended state rejected\n");
  } else if ((strcmp(name, "drop-next-tx") == 0 ||
              strcmp(name, "drop-next-rx") == 0) && arg1 != nullptr) {
    Supla::SupLan::RuntimeTestHooks *hooks = runtime->testHooks();
    const bool tx = strcmp(name, "drop-next-tx") == 0;
    uint8_t *hook = nullptr;
    if (strcmp(arg1, "ACK") == 0)
      hook = tx ? &hooks->dropNextAckTx : &hooks->dropNextAckRx;
    else if (strcmp(arg1, "DATA") == 0)
      hook = tx ? &hooks->dropNextDataTx : &hooks->dropNextDataRx;
    else if (strcmp(arg1, "FRAGMENT") == 0)
      hook = tx ? &hooks->dropNextFragmentTx : &hooks->dropNextFragmentRx;
    else if (strcmp(arg1, "SESSION_ACCEPT") == 0)
      hook = tx ? &hooks->dropNextSessionAcceptTx
                : &hooks->dropNextSessionAcceptRx;
    if (hook != nullptr) {
      *hook = 1;
      printf("OK hook=%s frame=%s\n", name, arg1);
    } else {
      printf("ERROR unknown frame kind\n");
    }
  } else if (strcmp(name, "corrupt-next-tx-tag") == 0) {
    runtime->testHooks()->corruptNextDataTagTx = 1;
    printf("OK hook=%s\n", name);
  } else if (strcmp(name, "corrupt-next-tx-mac") == 0) {
    runtime->testHooks()->corruptNextMacTx = 1;
    printf("OK hook=%s\n", name);
  } else if (strcmp(name, "corrupt-next-session-mac-tx") == 0) {
    runtime->testHooks()->corruptNextSessionMacTx = 1;
    printf("OK hook=%s\n", name);
  } else if (strcmp(name, "force-max-datagram") == 0 &&
             first >= 64 && first <= Supla::SupLan::kMaxDatagramPayload) {
    runtime->testHooks()->maxDatagramPayload = first;
    printf("OK max-datagram=%" PRIu32 "\n",
           static_cast<uint32_t>(first));
  } else if (strcmp(name, "drop-fragment-number") == 0 &&
             first <= UINT16_MAX) {
    runtime->testHooks()->dropFragmentNumber = first;
    printf("OK hook=drop-fragment-number value=%" PRIu32 "\n",
           static_cast<uint32_t>(first));
  } else if ((strcmp(name, "forget-session") == 0 ||
              strcmp(name, "clear-endpoint") == 0) && first <= 1) {
    if (strcmp(name, "forget-session") == 0) {
      printf(runtime->forgetSession(static_cast<uint8_t>(first))
                 ? "OK session forgotten\n" : "OK no active session\n");
    } else {
      runtime->clearEndpoint(static_cast<uint8_t>(first));
      printf("OK endpoint cleared\n");
    }
  } else if (strncmp(name, "flood-invalid-", 14) == 0 &&
             first <= 1000) {
    Supla::SupLan::TestFloodKind kind = Supla::SupLan::kFloodInvalidLocate;
    if (strcmp(name, "flood-invalid-session") == 0) {
      kind = Supla::SupLan::kFloodInvalidSession;
    } else if (strcmp(name, "flood-invalid-data") == 0) {
      kind = Supla::SupLan::kFloodInvalidData;
    } else if (strcmp(name, "flood-invalid-locate") != 0) {
      printf("ERROR unknown flood kind\n");
      return;
    }
    printf(runtime->startFlood(kind, primaryPeer,
                               static_cast<uint16_t>(first))
               ? "OK flood started\n" : "ERROR flood rejected\n");
  } else if (strcmp(name, "reset-test-counters") == 0) {
    runtime->resetDiagnostics();
    printf("OK counters reset\n");
  } else {
    printf("ERROR invalid command or arguments\n");
  }
}

static void wifiEvent(void *, esp_event_base_t base, int32_t id, void *) {
  if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
    xEventGroupClearBits(wifiEvents, kWifiConnected);
    (void)esp_wifi_connect();
  }
}

static void ipEvent(void *, esp_event_base_t, int32_t, void *eventData) {
  const ip_event_got_ip_t *event = static_cast<ip_event_got_ip_t *>(eventData);
  printf("WIFI_CONNECTED ip=" IPSTR "\n", IP2STR(&event->ip_info.ip));
  xEventGroupSetBits(wifiEvents, kWifiConnected);
}

static bool connectWifi() {
  if (CONFIG_SUPLAN_POC1_WIFI_SSID[0] == '\0') {
    printf("ERROR set Wi-Fi credentials with idf.py menuconfig\n");
    return false;
  }
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
      result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    if (nvs_flash_erase() != ESP_OK) return false;
    result = nvs_flash_init();
  }
  if (result != ESP_OK || esp_netif_init() != ESP_OK ||
      esp_event_loop_create_default() != ESP_OK ||
      esp_netif_create_default_wifi_sta() == nullptr) {
    return false;
  }
  wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&initConfig) != ESP_OK) return false;
  wifiEvents = xEventGroupCreate();
  if (wifiEvents == nullptr ||
      esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                 wifiEvent, nullptr) != ESP_OK ||
      esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                 ipEvent, nullptr) != ESP_OK) {
    return false;
  }
  wifi_config_t station = {};
  strncpy(reinterpret_cast<char *>(station.sta.ssid),
          CONFIG_SUPLAN_POC1_WIFI_SSID, sizeof(station.sta.ssid) - 1);
  strncpy(reinterpret_cast<char *>(station.sta.password),
          CONFIG_SUPLAN_POC1_WIFI_PASSWORD,
          sizeof(station.sta.password) - 1);
  station.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
      esp_wifi_set_config(WIFI_IF_STA, &station) != ESP_OK ||
      esp_wifi_start() != ESP_OK || esp_wifi_connect() != ESP_OK) {
    return false;
  }
  (void)xEventGroupWaitBits(wifiEvents, kWifiConnected, pdFALSE, pdTRUE,
                            portMAX_DELAY);
  return true;
}

static void poc1Task(void *) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (!connectWifi()) {
    printf("ERROR Wi-Fi initialization failed\n");
    vTaskDelete(nullptr);
    return;
  }
  if (!Supla::SupLan::Poc1::configurePeerTable(
          &crypto, &peers, kNodeA, &primaryPeer, &actionPeer) ||
      !datagrams.open(2016, Supla::SupLan::kMaxDatagramPayload)) {
    printf("ERROR static profile or UDP initialization failed\n");
    vTaskDelete(nullptr);
    return;
  }
  if (kNodeA) {
    relay = new Supla::Control::VirtualRelay();
    relay->setDefaultStateOff();
  } else {
    actionChannel = new Supla::AtChannel();
  }
    protocol = new (protocolStorage) Protocol(
      &SuplaDevice, &peers, localMapping(), 1, applicationEvent, nullptr);
  runtime = new (runtimeStorage) Runtime(
      &crypto, &randomPort, &datagrams, protocol, &peers,
      Supla::SupLan::Poc1::localNodeAddress(kNodeA),
      Supla::SupLan::kMinimumSuplaProtoVersion);
  protocol->attachRuntime(runtime);
  if (!protocol->verifyConfig()) {
    printf("ERROR invalid SupLAN ProtocolLayer configuration\n");
    vTaskDelete(nullptr);
    return;
  }
  int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  if (flags >= 0) (void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  printf("READY role=%s port=2016 multicast=239.255.201.6:2016"
         " peers=2 fixture=static\n", kRole);
  printf("Type help for PoC1 commands.\n");

  char line[192] = {};
  size_t lineLength = 0;
  bool running = true;
  uint32_t lastControlDispatch = 0;
  while (running) {
    runtime->iterate();
    const uint32_t controls = runtime->diagnostics().controlDispatched;
    if (controls != lastControlDispatch) {
      lastControlDispatch = controls;
      printf("EVENT CONTROL resource=50001 value=%u count=%" PRIu32 "\n",
             relay != nullptr && relay->isOn() ? 1 : 0, controls);
    }
    uint8_t input[64];
    const ssize_t amount = read(STDIN_FILENO, input, sizeof(input));
    if (amount > 0) {
      for (ssize_t i = 0; i < amount; ++i) {
        if (input[i] == '\n' || input[i] == '\r') {
          line[lineLength] = '\0';
          command(line, &running);
          lineLength = 0;
        } else if (lineLength + 1 < sizeof(line)) {
          line[lineLength++] = static_cast<char>(input[i]);
        } else {
          lineLength = 0;
          printf("ERROR command line too long\n");
        }
      }
    } else if (amount < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
               errno != EINTR) {
      printf("ERROR console read failed errno=%d\n", errno);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  datagrams.close();
  vTaskDelete(nullptr);
}

}  // namespace

extern "C" void app_main(void) {
  xTaskCreate(poc1Task, "suplan_poc1", 12288, nullptr, 5, nullptr);
}
