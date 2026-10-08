// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "suplan_runtime_linux.h"

#include <SuplaDevice.h>
#include <supla/at_channel.h>
#include <supla/channels/channel.h>
#include <supla/control/action_trigger.h>
#include <supla/control/hvac_base.h>
#include <supla/control/virtual_relay.h>
#include <supla/element.h>
#include <supla/log_wrapper.h>
#include <supla/network/network.h>
#include <supla/protocol/supla_srpc.h>
#include <suplan/suplan_config.h>
#include <suplan_poc1_profile.h>

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace Supla {

class LinuxSupLanRuntime::TransportLifecycle
    : public Protocol::SupLanTransportLifecycle {
 public:
  TransportLifecycle(SupLan::LinuxUdpPort* datagrams, uint16_t port,
                     const std::string& bindAddress)
      : datagrams_(datagrams), port_(port), bindAddress_(bindAddress) {}

  bool networkReady(uint32_t nowMs) override {
    if (!enabled_ || !Network::IsReady() || datagrams_ == nullptr) {
      haveMulticastProbe_ = false;
      return false;
    }
    if (!haveMulticastProbe_ ||
        static_cast<uint32_t>(nowMs - lastMulticastProbeMs_) >= 1000) {
      lastMulticastProbeMs_ = nowMs;
      haveMulticastProbe_ = true;
      multicastReady_ = datagrams_->hasActiveMulticastInterface();
    }
    return multicastReady_;
  }

  bool isOpen() const override {
    return datagrams_ != nullptr && datagrams_->isOpen();
  }

  bool open() override {
    if (datagrams_ == nullptr ||
        !datagrams_->open(bindAddress_.c_str(), port_,
                          SupLan::kMaxDatagramPayload)) {
      return false;
    }
    if (datagrams_->multicastInterfaceCount() == 0 ||
        datagrams_->joinedMulticastInterfaceCount() == 0) {
      datagrams_->close();
      return false;
    }
    return true;
  }

  void close() override {
    if (datagrams_ != nullptr) {
      datagrams_->close();
    }
  }

  void setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
      close();
    }
    resetOpenRetry();
  }

  bool isEnabled() const { return enabled_; }

 private:
  SupLan::LinuxUdpPort* datagrams_;
  uint16_t port_;
  std::string bindAddress_;
  uint32_t lastMulticastProbeMs_ = 0;
  bool haveMulticastProbe_ = false;
  bool multicastReady_ = false;
  bool enabled_ = true;
};

namespace {

void writeText(Debug::ResponseWriter* writer, const char* text) {
  if (writer != nullptr && text != nullptr) {
    writer->write(text);
  }
}

void putLe32(uint8_t* output, uint32_t value) {
  output[0] = static_cast<uint8_t>(value);
  output[1] = static_cast<uint8_t>(value >> 8);
  output[2] = static_cast<uint8_t>(value >> 16);
  output[3] = static_cast<uint8_t>(value >> 24);
}

bool parsePeer(std::istringstream* input, uint8_t maxPeers, uint8_t* peer) {
  unsigned value = 0;
  if (input == nullptr || peer == nullptr || !(*input >> value) ||
      value >= maxPeers) {
    return false;
  }
  *peer = static_cast<uint8_t>(value);
  return true;
}

}  // namespace

LinuxSupLanRuntime::LinuxSupLanRuntime(const LinuxSupLanConfig& config)
    : config_(config), nodeA_(config.role == 'A') {}

LinuxSupLanRuntime::~LinuxSupLanRuntime() = default;

bool LinuxSupLanRuntime::initialize() {
  if (!config_.enabled) return false;
  if (config_.serverProvisioning) {
    transport_.reset(new TransportLifecycle(
        &datagrams_, config_.unicastPort, config_.bindAddress));
    protocol_.reset(new Protocol::SupLan(
        &SuplaDevice, &peers_, nullptr, 0, onApplicationEvent, this));
    runtime_.reset(new SupLan::Runtime(&crypto_, &datagrams_, protocol_.get(),
                                       &peers_, {SupLan::kNodeIdDevice, 0},
                                       29));
    associations_.reset(
        new Device::ServerAssociations(&peers_, runtime_.get()));
    protocol_->attachRuntime(runtime_.get());
    protocol_->attachServerAssociations(associations_.get());
    protocol_->attachTransportLifecycle(transport_.get());
    return protocol_->verifyConfig();
  }
  if (config_.role != 'A' && config_.role != 'B') {
    return false;
  }
  if (!SupLan::Poc1::configurePeerTable(
          &peers_, nodeA_, &primaryPeer_, &actionPeer_)) {
    SUPLA_LOG_ERROR("SupLAN PoC fixture configuration failed");
    return false;
  }

  Element* element = Element::getElementByChannelNumber(0);
  if (nodeA_ && dynamic_cast<Control::VirtualRelay*>(element) == nullptr) {
    SUPLA_LOG_ERROR(
        "SupLAN role A requires a VirtualRelay on local channel 0");
    return false;
  }
  if (!nodeA_ && dynamic_cast<Control::ActionTrigger*>(element) == nullptr) {
    SUPLA_LOG_ERROR("SupLAN role B requires an Action Trigger on channel 0");
    return false;
  }

  mapping_ = nodeA_
      ? Protocol::SupLanResourceMapping{SupLan::Poc1::kRelayResourceId, 0,
                                        primaryPeer_, false}
      : Protocol::SupLanResourceMapping{SupLan::Poc1::kActionResourceId, 0,
                                        actionPeer_, true};
  transport_.reset(new TransportLifecycle(
      &datagrams_, config_.unicastPort, config_.bindAddress));
  protocol_.reset(new Protocol::SupLan(
      &SuplaDevice, &peers_, &mapping_, 1, onApplicationEvent, this));
  runtime_.reset(new SupLan::Runtime(
      &crypto_, &datagrams_, protocol_.get(), &peers_,
      SupLan::Poc1::localNodeAddress(nodeA_),
      SupLan::kMinimumSuplaProtoVersion));
  protocol_->attachRuntime(runtime_.get());
  protocol_->attachTransportLifecycle(transport_.get());
  if (!protocol_->verifyConfig()) {
    SUPLA_LOG_ERROR("SupLAN ProtocolLayer fixture validation failed");
    return false;
  }
  return true;
}

void LinuxSupLanRuntime::onApplicationEvent(
    void*, Protocol::SupLanApplicationEvent event, uint8_t peerIndex,
    const SupLan::ResourceId& resource, uint32_t messageType,
    const uint8_t* payload, size_t payloadLength) {
  if (event == Protocol::kSupLanRemoteState) {
    if (messageType == SupLan::kSuplaCallDeviceChannelValueChangedC &&
        payload != nullptr &&
        payloadLength == sizeof(TDS_SuplaDeviceChannelValue_C)) {
      TDS_SuplaDeviceChannelValue_C value = {};
      memcpy(&value, payload, sizeof(value));
      SUPLA_LOG_INFO("SupLAN STATE peer=%u resource=%" PRIu32
                     " bytes=%u channel=%u value0=%u",
                     peerIndex, resource.id,
                     static_cast<unsigned>(payloadLength),
                     value.ChannelNumber,
                     static_cast<unsigned char>(value.value[0]));
    } else {
      SUPLA_LOG_INFO("SupLAN STATE peer=%u resource=%" PRIu32 " bytes=%u",
                     peerIndex, resource.id,
                     static_cast<unsigned>(payloadLength));
    }
  } else if (event == Protocol::kSupLanRemoteAction) {
    const uint32_t actionId = payload != nullptr && payloadLength >= 5
        ? static_cast<uint32_t>(payload[1]) |
            (static_cast<uint32_t>(payload[2]) << 8) |
            (static_cast<uint32_t>(payload[3]) << 16) |
            (static_cast<uint32_t>(payload[4]) << 24)
        : messageType;
    SUPLA_LOG_INFO("SupLAN ACTION peer=%u resource=%" PRIu32
                   " action=%" PRIu32,
                   peerIndex, resource.id, actionId);
  } else if (event == Protocol::kSupLanOperationAck && payload != nullptr &&
             payloadLength == 5) {
    const uint32_t sequence = static_cast<uint32_t>(payload[0]) |
        (static_cast<uint32_t>(payload[1]) << 8) |
        (static_cast<uint32_t>(payload[2]) << 16) |
        (static_cast<uint32_t>(payload[3]) << 24);
    SUPLA_LOG_INFO("SupLAN ACK peer=%u resource=%" PRIu32
                   " sequence=%" PRIu32 " result=%u",
                   peerIndex, resource.id, sequence, payload[4]);
  } else if (event == Protocol::kSupLanReadInterest) {
    SUPLA_LOG_INFO("SupLAN READ peer=%u resource=%" PRIu32
                   " event_only=%u",
                   peerIndex, resource.id, messageType);
  }
}

bool LinuxSupLanRuntime::processDebugCommand(
    const char* line, Debug::ResponseWriter* writer) {
  if (line == nullptr || line[0] == '\0' || protocol_ == nullptr ||
      runtime_ == nullptr) {
    return false;
  }

  std::istringstream input(line);
  std::string command;
  input >> command;
  if (command == "suplan-help") {
    writeText(writer,
        "SupLAN commands: show-status, show-resources, show-counters, "
        "show-pools, read <resource>, control <resource> <0|1>, "
        "set-resource-value <resource> <0|1>, emit-action <resource> "
        "<action>, forget-session <peer>, clear-endpoint <peer>, "
        "transport <on|off>, mtu <48..250> (test), "
        "fault <drop-ack-rx|drop-fragment-tx|corrupt-data-tag> <0..3>\n");
  } else if (command == "fault") {
    std::string kind;
    unsigned count = 0;
    uint8_t* counter = nullptr;
    if (input >> kind >> count && count <= 3) {
      auto* hooks = runtime_->testHooks();
      if (kind == "drop-ack-rx") {
        counter = &hooks->dropNextAckRx;
      } else if (kind == "drop-fragment-tx") {
        counter = &hooks->dropNextFragmentTx;
      } else if (kind == "corrupt-data-tag") {
        counter = &hooks->corruptNextDataTagTx;
      }
    }
    if (counter == nullptr) {
      writeText(writer, "ERROR expected supported fault and count 0..3\n");
    } else {
      *counter = static_cast<uint8_t>(count);
      writeText(writer, "OK bounded test fault applied\n");
    }
  } else if (command == "mtu") {
    unsigned mtu = 0;
    if (!(input >> mtu) || mtu < 48 || mtu > SupLan::kMaxDatagramPayload) {
      writeText(writer, "ERROR expected MTU 48..250\n");
    } else {
      runtime_->testHooks()->maxDatagramPayload = mtu;
      writeText(writer, "OK test MTU applied\n");
    }
  } else if (command == "show-status") {
    writeStatus(writer);
  } else if (command == "show-resources") {
    if (config_.serverProvisioning) {
      for (auto channel = Channel::Begin(); channel;
           channel = channel->next()) {
        char output[128] = {};
        snprintf(output, sizeof(output), "RESOURCE id=%" PRIu32
                 " channel=%d type=%" PRIu32 "\n",
                 channel->getServerChannelId(), channel->getChannelNumber(),
                 channel->getChannelType());
        writeText(writer, output);
      }
      return true;
    }
    const char* role = nodeA_ ? "A" : "B";
    if (nodeA_) {
      auto* relay = dynamic_cast<Control::VirtualRelay*>(
          Element::getElementByChannelNumber(0));
      char output[160] = {};
      snprintf(output, sizeof(output),
               "RESOURCES role=%s 50001=relay:%u channel=0\n", role,
               relay != nullptr && relay->isOn() ? 1U : 0U);
      writeText(writer, output);
    } else {
      char output[160] = {};
      snprintf(output, sizeof(output),
               "RESOURCES role=%s 50002=action-trigger channel=0\n", role);
      writeText(writer, output);
    }
  } else if (command == "show-counters") {
    writeCounters(writer);
  } else if (command == "show-pools") {
    writePools(writer);
  } else if (command == "sleep-hint" || command == "wake-hint") {
    unsigned duration = 0;
    if (!(input >> duration) || duration == 0 || duration > UINT16_MAX) {
      writeText(writer, "ERROR expected duration 1..65535\n");
    } else if (command == "sleep-hint") {
      protocol_->announceSleep(duration);
      writeText(writer, "OK software sleep hint queued\n");
    } else {
      protocol_->beginWake(duration, true);
      writeText(writer, "OK retained-interest wake queued\n");
    }
  } else if (command == "hvac-config" || command == "hvac-upload") {
    unsigned number = 0;
    unsigned mainId = 0;
    unsigned minOn = 0;
    auto *srpc = SuplaDevice.getSrpcLayer();
    if (!(input >> number) || number >= SUPLA_CHANNELMAXCOUNT) {
      writeText(writer, "ERROR expected local HVAC channel number\n");
      return true;
    }
    auto *hvac = dynamic_cast<Control::HvacBase *>(
        Element::getElementByChannelNumber(number));
    TChannelConfig_HVAC config = {};
    int size = 0;
    if (hvac != nullptr) {
      hvac->fillChannelConfig(&config, &size, SUPLA_CONFIG_TYPE_DEFAULT);
    }
    if (size != sizeof(config)) {
      writeText(writer, "ERROR HVAC serialization unavailable\n");
    } else if (command == "hvac-config") {
      char output[128] = {};
      snprintf(output, sizeof(output),
               "HVAC main=%" PRIu32 " min_on=%u temp=%d mode=%d "
               "heat=%u setpoint=%d\n",
               config.MainThermometerChannelId,
               static_cast<unsigned>(config.MinOnTimeS),
               static_cast<int>(hvac->getLastTemperature()), hvac->getMode(),
               hvac->getChannel()->isHvacFlagHeating() ? 1U : 0U,
               hvac->getTemperatureSetpointHeat());
      writeText(writer, output);
    } else if (!(input >> mainId >> minOn) || mainId > INT32_MAX ||
               minOn > UINT16_MAX || srpc == nullptr || !srpc->isConnected()) {
      writeText(writer, "ERROR expected main ChannelId and MinOnTimeS\n");
    } else {
      // Insecure Linux diagnostics only: send a full SDK-serialized request
      // through normal SRPC. Do not activate the candidate or bypass Server
      // protected merge; the authoritative echo remains the config source.
      config.MainThermometerChannelId = mainId;
      config.MinOnTimeS = minOn;
      writeText(writer, srpc->setChannelConfig(
          number, hvac->getChannel()->getDefaultFunction(), &config, size,
          SUPLA_CONFIG_TYPE_DEFAULT) ? "OK HVAC upload queued\n" :
                                      "ERROR HVAC upload rejected\n");
    }
  } else if (command == "transport") {
    std::string state;
    if (!(input >> state) || (state != "on" && state != "off")) {
      writeText(writer, "ERROR expected transport on or off\n");
    } else {
      setTransportEnabledForTest(state == "on");
      writeText(writer, state == "on" ? "OK SupLAN transport enabled\n"
                                       : "OK SupLAN transport disabled\n");
    }
  } else if (command == "read") {
    uint32_t resource = 0;
    if (!(input >> resource)) {
      writeText(writer, "ERROR expected resource id\n");
    } else {
      writeText(writer, debugRead(resource) ? "OK read queued\n"
                                            : "ERROR read rejected\n");
    }
  } else if (command == "control") {
    uint32_t resource = 0;
    uint32_t value = 0;
    if (!(input >> resource >> value)) {
      writeText(writer, "ERROR expected resource id and value 0 or 1\n");
    } else {
      writeText(writer, debugControl(resource, value)
                            ? "OK control queued\n"
                            : "ERROR control rejected\n");
    }
  } else if (command == "set-resource-value") {
    uint32_t resource = 0;
    uint32_t value = 0;
    if (!(input >> resource >> value) || !nodeA_ ||
        resource != SupLan::Poc1::kRelayResourceId || value > 1) {
      writeText(writer,
          "ERROR expected local relay resource 50001 and value 0 or 1\n");
    } else {
      auto* relay = dynamic_cast<Control::VirtualRelay*>(
          Element::getElementByChannelNumber(0));
      if (relay == nullptr) {
        writeText(writer, "ERROR local relay channel missing\n");
      } else {
        if (value != 0) {
          relay->turnOn();
        } else {
          relay->turnOff();
        }
        Channel* channel = Channel::GetByChannelNumber(0);
        if (channel != nullptr) {
          channel->sendUpdate();
        }
        writeText(writer, "OK local SUPLA Channel updated\n");
      }
    }
  } else if (command == "emit-action") {
    uint32_t resource = 0;
    uint32_t actionId = 0;
    if (!(input >> resource >> actionId) || nodeA_ ||
        resource != SupLan::Poc1::kActionResourceId || actionId >= 32) {
      writeText(writer,
          "ERROR expected local action resource 50002 and action id 0..31\n");
    } else {
      auto* actionChannel = dynamic_cast<AtChannel*>(
          Channel::GetByChannelNumber(0));
      if (actionChannel == nullptr) {
        writeText(writer, "ERROR local Action Trigger channel missing\n");
      } else {
        actionChannel->pushAction(1U << actionId);
        actionChannel->sendUpdate();
        writeText(writer, "OK action queued through local Channel\n");
      }
    }
  } else if (command == "forget-session" || command == "clear-endpoint") {
    uint8_t peer = 0;
    if (!parsePeer(&input, SUPLAN_MAX_PERSISTENT_PEERS, &peer) ||
        peers_.get(peer) == nullptr) {
      writeText(writer, "ERROR invalid peer index\n");
    } else if (command == "forget-session") {
      writeText(writer, runtime_->forgetSession(peer)
                            ? "OK session forgotten\n"
                            : "ERROR session not found\n");
    } else {
      runtime_->clearEndpoint(peer);
      writeText(writer, "OK endpoint cleared\n");
    }
  } else {
    return false;
  }
  return true;
}

bool LinuxSupLanRuntime::debugRead(uint32_t resourceId) {
  const uint8_t peer = resourceId == SupLan::Poc1::kActionResourceId
      ? actionPeer_ : primaryPeer_;
  const SupLan::ResourceId resource = {
      SupLan::kResourceTypeChannel, resourceId};
  if (config_.serverProvisioning) {
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      if (runtime_->requestRead(i, resource)) return true;
    }
    return false;
  }
  return runtime_->requestRead(peer, resource);
}

bool LinuxSupLanRuntime::debugControl(uint32_t resourceId, uint32_t value) {
  if (value > 1) {
    return false;
  }
  const uint8_t peer = primaryPeer_;
  const SupLan::ResourceId resource = {
      SupLan::kResourceTypeChannel, resourceId};
  uint8_t payload[17] = {};
  putLe32(payload, 1);
  payload[4] = SupLan::kChannelNumberUnresolved;
  putLe32(payload + 5, 0);
  payload[9] = static_cast<uint8_t>(value);
  if (config_.serverProvisioning) {
    for (uint16_t i = 0; i < SUPLAN_MAX_PERSISTENT_PEERS; ++i) {
      if (runtime_->sendControl(i, resource, payload, sizeof(payload)))
        return true;
    }
    return false;
  }
  return runtime_->sendControl(peer, resource, payload, sizeof(payload));
}

void LinuxSupLanRuntime::writeStatus(Debug::ResponseWriter* writer) const {
  char output[256] = {};
  const bool srpcReady = SuplaDevice.getSrpcLayer() != nullptr &&
      SuplaDevice.getSrpcLayer()->isRegisteredAndReady();
  const bool srpcConnected = SuplaDevice.getSrpcLayer() != nullptr &&
      SuplaDevice.getSrpcLayer()->isConnected();
  const int deviceStatus = SuplaDevice.getCurrentStatus();
  snprintf(output, sizeof(output),
           "STATUS network_ready=%u srpc_connected=%u srpc_registered=%u "
           "device_status=%d "
           "suplan_enabled=%u "
           "transport=%s transport_enabled=%u sessions=%u role=%c "
           "port=%u\n",
           Network::IsReady() ? 1U : 0U, srpcConnected ? 1U : 0U,
           srpcReady ? 1U : 0U, deviceStatus,
           protocol_ != nullptr && protocol_->isEnabled() ? 1U : 0U,
           isTransportOpen() ? "open" : "closed",
           transport_ != nullptr && transport_->isEnabled() ? 1U : 0U,
           static_cast<unsigned>(runtime_->poolDiagnostics().sessions.used),
           config_.serverProvisioning ? 'S' : config_.role,
           config_.unicastPort);
  writeText(writer, output);
}

void LinuxSupLanRuntime::writeCounters(Debug::ResponseWriter* writer) const {
  const SupLan::Diagnostics& d = runtime_->diagnostics();
  char output[1200] = {};
  snprintf(output, sizeof(output),
      "COUNTERS locate_tx=%" PRIu32 " locate_rx=%" PRIu32
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
      " acl_reject=%" PRIu32 " resource_not_found=%" PRIu32
      " invalid_locate=%" PRIu32 " invalid_session=%" PRIu32
      " invalid_data=%" PRIu32 " deferred_queue_overflow=%" PRIu32
      " pool_reject=%" PRIu32 "\n",
      d.locateTx, d.locateRx, d.locateReplyTx, d.locateReplyRx,
      d.sessionInitTx, d.sessionInitRx, d.sessionAcceptTx, d.sessionAcceptRx,
      d.sessionEstablished, d.sessionReplaced, d.dataTx, d.dataRx,
      d.dataAuthFail, d.dataReplayDrop, d.dataDuplicate, d.ackTx, d.ackRx,
      d.retryTx, d.controlDispatched, d.controlDuplicateSuppressed,
      d.readDispatched, d.stateNotificationTx, d.stateNotificationRx,
      d.actionTx, d.actionRx, d.fragmentTx, d.fragmentRx,
      d.reassemblyStarted, d.reassemblyCompleted, d.reassemblyExpired,
      d.reassemblyRejected, d.aclReject, d.resourceNotFound,
      d.invalidLocateDrop, d.invalidSessionDrop, d.invalidDataDrop,
      d.deferredQueueOverflow, d.poolReject);
  writeText(writer, output);
}

void LinuxSupLanRuntime::writePools(Debug::ResponseWriter* writer) const {
  const SupLan::PoolDiagnostics p = runtime_->poolDiagnostics();
  char output[768] = {};
  snprintf(output, sizeof(output),
      "POOLS peers=%u/%u(high=%u) aclEntries=%u/%u(high=%u) "
      "sessions=%u/%u(high=%u) pending=%u/%u(high=%u) "
      "locates=%u/%u(high=%u) interests=%u/%u(high=%u) "
      "reassembly=%u/%u(high=%u) retries=%u/%u(high=%u) "
      "deferredEvents=%u/%u(high=%u) workspace_bytes=%u\n",
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
  writeText(writer, output);
}

Protocol::SupLan* LinuxSupLanRuntime::protocol() const {
  return protocol_.get();
}

bool LinuxSupLanRuntime::isTransportOpen() const {
  return transport_ != nullptr && transport_->isOpen();
}

bool LinuxSupLanRuntime::setTransportEnabledForTest(bool enabled) {
  if (transport_ == nullptr) {
    return false;
  }
  transport_->setEnabled(enabled);
  return true;
}

}  // namespace Supla
