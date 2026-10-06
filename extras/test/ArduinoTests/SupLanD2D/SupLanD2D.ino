// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <supla-common/srpc.h>
#include <supla/control/virtual_relay.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sha256.h>
#include "arduino_ports.h"
#include "crypto_selftest.h"
#include "suplan_poc1_credentials.h"


static_assert(SUPLA_PROTO_VERSION == 29, "SupLAN protocol version");
static_assert(SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED == 0x80000,
              "SupLAN capability flag");
static_assert(sizeof(TSuplaSuplanPeerContext) == 27, "PeerContext wire size");
static_assert(sizeof(TSuplaSuplanResource) == 5, "Resource wire size");
static_assert(sizeof(TSuplaSuplanAclEntry) == 6, "ACL wire size");
static_assert(sizeof(TSD_SuplaDeviceIdentities) == 520, "Identities wire size");
static_assert(sizeof(TDS_SuplaDeviceIdentitiesResult) == 5,
              "Identities result wire size");
static_assert(sizeof(TSDS_SuplaSetSuplanSourceAssociation) == 568,
              "Source wire size");
static_assert(sizeof(TDS_SuplaSetSuplanSourceAssociationResult) == 65,
              "Source result wire size");
static_assert(sizeof(TSDS_SuplaSetSuplanDestinationAssociation) == 600,
              "Destination wire size");
static_assert(sizeof(TDS_SuplaSetSuplanDestinationAssociationResult) == 32,
              "Destination result wire size");
static_assert(sizeof(TDS_SuplaEnsureResourceAccess) == 8, "Ensure wire size");
static_assert(sizeof(TSD_SuplaEnsureResourceAccessResult) == 3,
              "Ensure result wire size");
static_assert(offsetof(TDS_SuplaEnsureResourceAccess, DeliveryMode) == 6,
              "Ensure delivery offset");
static_assert(offsetof(TDS_SuplaEnsureResourceAccess, Flags) == 7,
              "Ensure flags offset");
static_assert(offsetof(TSD_SuplaEnsureResourceAccessResult, DeliveryMode) == 2,
              "Ensure result delivery offset");
static_assert(sizeof(TSD_SuplaRemoteChannelState) == 40,
              "Remote Channel snapshot wire size");
static_assert(offsetof(TSD_SuplaRemoteChannelState, Channel) == 4,
              "Remote Channel snapshot offset");
static_assert(sizeof(TSDS_SuplaSetSuplanDestinationAssociation) <=
              SUPLA_MAX_DATA_SIZE, "Device payload bound");
const unsigned int validationCalls[] = {
    SUPLA_SD_CALL_SUPLAN_DEVICE_IDENTITIES,
    SUPLA_DS_CALL_SUPLAN_DEVICE_IDENTITIES_RESULT,
    SUPLA_SD_CALL_SET_SUPLAN_SOURCE_ASSOCIATION,
    SUPLA_DS_CALL_SET_SUPLAN_SOURCE_ASSOCIATION_RESULT,
    SUPLA_SD_CALL_SET_SUPLAN_DESTINATION_ASSOCIATION,
    SUPLA_DS_CALL_SET_SUPLAN_DESTINATION_ASSOCIATION_RESULT,
    SUPLA_DS_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS,
    SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS_RESULT,
    SUPLA_SD_CALL_REMOTE_CHANNEL_STATE};

Supla::LittleFsConfig validationConfig;
Supla::ESPWifi validationWifi;
Supla::EspWebServer validationWebServer;
Supla::Control::VirtualRelay *validationRelay = nullptr;
uint32_t lastSample = 0;
uint32_t minimumHeap = UINT32_MAX;
char command[96] = {};
size_t commandSize = 0;

ArduinoCrypto d2dCrypto;
ArduinoUdp d2dUdp;
Supla::SupLan::PeerTable d2dPeers;
const Supla::Protocol::SupLanResourceMapping d2dMapping = {50001, 7, 0, false};
Supla::Protocol::SupLan *d2dProtocol = nullptr;
Supla::SupLan::Runtime *d2dRuntime = nullptr;

void d2dStatus() {
  const auto &d = d2dRuntime->diagnostics();
  Serial.printf("VALIDATION D2D transport=%d sessions=%u locate_rx=%u "
                "locate_reply_tx=%u session_established=%u reads=%u "
                "controls=%u states_tx=%u auth_fail=%u acl_reject=%u "
                "ack_tx=%u duplicates=%u replay_drop=%u invalid_locate=%u "
                "invalid_session=%u udp_unicast=%u udp_multicast=%u "
                "udp_oversize=%u actions_rx=%u radio_sleep=%u\n",
                d2dUdp.isOpen(), d2dRuntime->poolDiagnostics().sessions.used,
                d.locateRx, d.locateReplyTx, d.sessionEstablished,
                d.readDispatched, d.controlDispatched, d.stateNotificationTx,
                d.dataAuthFail, d.aclReject, d.ackTx,
                d.controlDuplicateSuppressed, d.dataReplayDrop,
                d.invalidLocateDrop, d.invalidSessionDrop,
                d2dUdp.rxUnicast, d2dUdp.rxMulticast, d2dUdp.rxOversize,
                d.actionRx,
#ifdef ARDUINO_ARCH_ESP8266
                static_cast<unsigned>(WiFi.getSleepMode())
#else
                static_cast<unsigned>(WiFi.getSleep())
#endif
                );
}

void d2dEvent(void *, Supla::Protocol::SupLanApplicationEvent event,
              uint8_t, const Supla::SupLan::ResourceId &resource, uint32_t,
              const uint8_t *payload, size_t length) {
  if (event == Supla::Protocol::kSupLanRemoteAction && length >= 5) {
    const uint32_t action = static_cast<uint32_t>(payload[1]) |
        static_cast<uint32_t>(payload[2]) << 8 |
        static_cast<uint32_t>(payload[3]) << 16 |
        static_cast<uint32_t>(payload[4]) << 24;
    Serial.printf("VALIDATION ACTION resource=%u action=%u\n",
                  resource.id, action);
  }
}

void sample() {
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < minimumHeap) {
    minimumHeap = heap;
  }
#ifdef ARDUINO_ARCH_ESP8266
  const uint32_t largest = ESP.getMaxFreeBlockSize();
  const uint32_t fragmentation = ESP.getHeapFragmentation();
#else
  const uint32_t largest = ESP.getMaxAllocHeap();
  const uint32_t fragmentation = 0;
#endif
  auto *srpc = SuplaDevice.getSrpcLayer();
  Serial.printf("VALIDATION SAMPLE ms=%u heap=%u min=%u block=%u frag=%u "
                "status=%d wifi=%d ready=%d relay=%d epoch=%u identity=%d "
                "transition=%d sync=%d\n",
                static_cast<unsigned>(millis()), static_cast<unsigned>(heap),
                static_cast<unsigned>(minimumHeap),
                static_cast<unsigned>(largest),
                static_cast<unsigned>(fragmentation),
                SuplaDevice.getCurrentStatus(), validationWifi.isReady(),
                srpc->isRegisteredAndReady(), validationRelay->isOn(),
                static_cast<unsigned>(srpc->serverIdentity().rootEpoch()),
                srpc->serverIdentity().identityAvailable(),
                srpc->serverIdentity().identityTransition(),
                srpc->serverIdentity().serverSyncComplete());
}

void processCommand() {
  auto *srpc = SuplaDevice.getSrpcLayer();
  if (strcmp(command, "d2d-read-action") == 0) {
    const Supla::SupLan::ResourceId resource = {1, 50002};
    Serial.printf("VALIDATION D2D_READ_ACTION ok=%d\n",
                  d2dRuntime->requestRead(1, resource));
  } else if (strcmp(command, "d2d-no-sleep") == 0) {
#ifdef ARDUINO_ARCH_ESP8266
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif
  } else if (strcmp(command, "d2d-status") == 0) {
    d2dStatus();
  } else if (strcmp(command, "d2d-forget") == 0) {
    Serial.printf("VALIDATION D2D_FORGET ok=%d\n", d2dRuntime->forgetSession(0));
  } else if (strcmp(command, "d2d-off") == 0) {
    d2dUdp.setEnabled(false);
  } else if (strcmp(command, "d2d-on") == 0) {
    d2dUdp.setEnabled(true);
  } else if (strcmp(command, "sample") == 0) {
    sample();
  } else if (strcmp(command, "reconnect") == 0) {
    srpc->disconnect();
    Serial.println("VALIDATION RECONNECT requested");
  } else if (strcmp(command, "wifi-reconnect") == 0) {
    Supla::Network::DisconnectProtocols();
    WiFi.disconnect();
    Serial.println("VALIDATION WIFI_RECONNECT requested");
  } else if (strcmp(command, "relay-on") == 0) {
    validationRelay->turnOn();
  } else if (strcmp(command, "relay-off") == 0) {
    validationRelay->turnOff();
  } else if (strcmp(command, "restart") == 0) {
    Serial.println("VALIDATION RESTART requested");
    SuplaDevice.softRestart();
  } else if (strncmp(command, "identity ", 9) == 0) {
    // Test injection into the real component, not a Server wire exchange.
    long deviceId = 0;
    long channelId = 0;
    if (sscanf(command + 9, "%ld %ld", &deviceId, &channelId) == 2) {
      TSD_SuplaDeviceIdentities snapshot = {};
      snapshot.DeviceId = deviceId;
      snapshot.ChannelCount = 1;
      snapshot.ChannelId[0] = channelId;
      const auto result = srpc->serverIdentity().accept(snapshot);
      Serial.printf("VALIDATION IDENTITY result=%u epoch=%u\n",
                    static_cast<unsigned>(result.Result),
                    static_cast<unsigned>(result.RootEpoch));
      const auto local = srpc->serverIdentity().resolve(channelId);
      uint32_t reversed = 0;
      const bool reverseOk = srpc->serverIdentity().reverse(7, &reversed);
      Serial.printf("VALIDATION RESOLVE location=%u number=%u reverse=%d "
                    "id=%ld\n", static_cast<unsigned>(local.location),
                    static_cast<unsigned>(local.channelNumber), reverseOk,
                    static_cast<long>(reversed));
    }
  } else {
    Serial.println("VALIDATION COMMAND invalid");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("VALIDATION BOOT");
  bool protocolOk = true;
  for (unsigned int i = 0; i < 8; ++i) {
    protocolOk = protocolOk && validationCalls[i] == 1280 + 10 * i &&
                 srpc_call_min_version_required(nullptr, validationCalls[i]) == 29;
  }
  protocolOk = protocolOk && validationCalls[8] == 1380 &&
               srpc_call_min_version_required(nullptr, validationCalls[8]) == 29;
  Serial.printf("VALIDATION PROTO ok=%d\n", protocolOk);
  Supla::Sha256 hash;
  const uint8_t input[] = {'a', 'b', 'c'};
  uint8_t digest[32] = {};
  hash.update(input, sizeof(input));
  hash.digest(digest);
  const uint8_t expected[32] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
  Serial.printf("VALIDATION SHA256 ok=%d\n",
                hash.isValid() && memcmp(digest, expected, 32) == 0);
  validationRelay = new Supla::Control::VirtualRelay(
      SUPLA_BIT_FUNC_LIGHTSWITCH);
  validationRelay->getChannel()->setChannelNumber(7);
  validationRelay->setDefaultFunction(SUPLA_CHANNELFNC_LIGHTSWITCH);
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setName("SupLAN Arduino validation");
  SuplaDevice.setSwVersion("SupLAN-D2D-Arduino");
  SuplaDevice.setAutomaticResetOnConnectionProblem(0);
  const bool cryptoOk = cryptoSelfTest(&d2dCrypto);
  Serial.printf("VALIDATION D2D_CRYPTO ok=%d\n", cryptoOk);
  if (!cryptoOk) {
    return;
  }
  uint8_t primary = 0;
  uint8_t action = 1;
  const bool peersOk = Supla::SupLan::Poc1::configurePeerTable(&d2dPeers, true, &primary, &action);
  const bool kdfOk = peersOk && d2dPeers.get(primary) != nullptr &&
      memcmp(d2dPeers.get(primary)->peerKey,
             Supla::SupLan::Poc1::kPrimaryPeerKey, 32) == 0;
  Serial.printf("VALIDATION D2D_KDF ok=%d\n", kdfOk);
  if (!kdfOk) {
    return;
  }
  d2dProtocol = new Supla::Protocol::SupLan(
      &SuplaDevice, &d2dPeers, &d2dMapping, 1, d2dEvent);
  d2dRuntime = new Supla::SupLan::Runtime(
      &d2dCrypto, &d2dUdp, d2dProtocol, &d2dPeers,
      Supla::SupLan::Poc1::localNodeAddress(true), SUPLA_PROTO_VERSION);
  d2dProtocol->attachRuntime(d2dRuntime);
  d2dProtocol->attachTransportLifecycle(&d2dUdp);
  Serial.printf("VALIDATION D2D_INIT ok=%d runtime_bytes=%u peers_bytes=%u\n",
                peersOk && d2dProtocol->verifyConfig(),
                static_cast<unsigned>(sizeof(Supla::SupLan::Runtime)),
                static_cast<unsigned>(sizeof(Supla::SupLan::PeerTable)));
  const bool initialized = SuplaDevice.begin(SUPLA_PROTO_VERSION);
  Serial.printf("VALIDATION INIT ok=%d srpc_size=%u identity_size=%u "
                "flag=%d\n", initialized,
                static_cast<unsigned>(sizeof(Supla::Protocol::SuplaSrpc)),
                static_cast<unsigned>(sizeof(Supla::Device::ServerIdentity)),
                (Supla::RegisterDevice::getRegDevHeaderPtr()->Flags &
                 SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED) != 0);
}

void loop() {
  if (d2dRuntime == nullptr) {
    delay(100);
    return;
  }
  SuplaDevice.iterate();
  if (millis() - lastSample >= 5000) {
    lastSample = millis();
    sample();
  }
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n') {
      command[commandSize] = 0;
      processCommand();
      commandSize = 0;
    } else if (c != '\r') {
      if (commandSize + 1 < sizeof(command)) {
        command[commandSize++] = c;
      } else {
        commandSize = 0;
      }
    }
  }
}
