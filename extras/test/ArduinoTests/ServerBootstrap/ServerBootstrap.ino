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
static_assert(sizeof(TDS_SuplaEnsureResourceAccess) == 7, "Ensure wire size");
static_assert(sizeof(TSD_SuplaEnsureResourceAccessResult) == 2,
              "Ensure result wire size");
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
    SUPLA_SD_CALL_ENSURE_SUPLAN_RESOURCE_ACCESS_RESULT};

Supla::LittleFsConfig validationConfig;
Supla::ESPWifi validationWifi;
Supla::EspWebServer validationWebServer;
Supla::Control::VirtualRelay *validationRelay = nullptr;
uint32_t lastSample = 0;
uint32_t minimumHeap = UINT32_MAX;
char command[96] = {};
size_t commandSize = 0;

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
  if (strcmp(command, "sample") == 0) {
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
  SuplaDevice.setSwVersion("M1B-SD1-Arduino");
  SuplaDevice.setAutomaticResetOnConnectionProblem(0);
  const bool initialized = SuplaDevice.begin(SUPLA_PROTO_VERSION);
  Serial.printf("VALIDATION INIT ok=%d srpc_size=%u identity_size=%u "
                "flag=%d\n", initialized,
                static_cast<unsigned>(sizeof(Supla::Protocol::SuplaSrpc)),
                static_cast<unsigned>(sizeof(Supla::Device::ServerIdentity)),
                (Supla::RegisterDevice::getRegDevHeaderPtr()->Flags &
                 SUPLA_DEVICE_FLAG_SUPLAN_SUPPORTED) != 0);
}

void loop() {
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
