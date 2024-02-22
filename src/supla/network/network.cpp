/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include <string.h>
#include <stdio.h>
#include <supla/time.h>

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/protocol/protocol_layer.h>
#include <supla/element.h>
#include <supla/network/network.h>
#include <supla/storage/config.h>
#include <supla/tools.h>
#include <supla/protocol/supla_srpc.h>

#include "client.h"

namespace Supla {

Network *Network::netIntf = nullptr;
Network *Network::firstNetIntf = nullptr;
enum DeviceMode Network::mode = DEVICE_MODE_NORMAL;

Network *Network::Instance() {
  return netIntf;
}

Network *Network::FirstInstance() {
  return firstNetIntf;
}

Network *Network::NextInstance(Network *instance) {
  if (instance) {
    return instance->nextNetIntf;
  }
  return nullptr;
}

Network *Network::GetInstanceByIP(uint32_t ip) {
  auto ptr = firstNetIntf;
  while (ptr) {
    if (ip == ptr->getIP()) {
      return ptr;
    }
    ptr = ptr->nextNetIntf;
  }
  return nullptr;
}

int Network::GetNetIntfCount() {
  int count = 0;
  auto ptr = firstNetIntf;
  while (ptr) {
    count++;
    ptr = ptr->nextNetIntf;
  }
  return count;
}

void Network::DisconnectProtocols() {
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
      proto = proto->next()) {
    proto->disconnect();
  }
}

void Network::Setup() {
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      ptr->setup();
    }
    ptr = ptr->nextNetIntf;
  }
}

void Network::Disable() {
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      ptr->disable();
    }
    ptr = ptr->nextNetIntf;
  }
}

void Network::Uninit() {
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      ptr->uninit();
    }
    ptr = ptr->nextNetIntf;
  }
}

bool Network::IsReady() {
  auto ptr = firstNetIntf;
  bool result = false;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      if (ptr->isReady()) {
        result = true;
      }
    }
    ptr = ptr->nextNetIntf;
  }
  return result;
}

bool Network::Iterate() {
  bool result = false;
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      if (ptr->iterate()) {
        result = true;
      }
    }
    ptr = ptr->nextNetIntf;
  }
  return result;
}

void Network::SetConfigMode() {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->setConfigMode();
    ptr = ptr->nextNetIntf;
  }
}

void Network::SetNormalMode() {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->setNormalMode();
    ptr = ptr->nextNetIntf;
  }
}

void Network::SetSetupNeeded() {
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      ptr->setSetupNeeded();
    }
    ptr = ptr->nextNetIntf;
  }
}

bool Network::PopSetupNeeded() {
  bool setupNeeded = false;
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig()) {
      if (ptr->popSetupNeeded()) {
        setupNeeded = true;
      }
    }
    ptr = ptr->nextNetIntf;
  }
  return setupNeeded;
}

bool Network::GetMainMacAddr(uint8_t *buf) {
  auto ptr = firstNetIntf;
  if (ptr == nullptr) {
    return false;
  }
  // Returns WiFi MAC address by default. If WiFi interface is missing, then
  // returns first Network instance MAC address
  while (ptr) {
    if (ptr->getIntfType() == IntfType::WiFi) {
      return ptr->getMacAddr(buf);
    }
    ptr = ptr->nextNetIntf;
  }
  return firstNetIntf->getMacAddr(buf);
}

void Network::SetHostname(const char *buf, int macSize) {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->setHostname(buf, macSize);
    ptr = ptr->nextNetIntf;
  }
}

bool Network::IsIpSetupTimeout() {
  bool result = true;
  auto ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIpSetupTimeout()) {
      result = false;
    }
    ptr = ptr->nextNetIntf;
  }
  return result;
}

void Network::LoadConfig() {
  auto ptr = firstNetIntf;
  Network *mainIntf = netIntf;
  while (ptr) {
    ptr->onLoadConfig();
    if (ptr->isIntfDisabledInConfig() && ptr == mainIntf) {
      mainIntf = nullptr;
    }
    ptr = ptr->nextNetIntf;
  }

  ptr = firstNetIntf;
  while (ptr) {
    if (!ptr->isIntfDisabledInConfig() && mainIntf == nullptr) {
      mainIntf = ptr;
    }
    if (!ptr->isIntfDisabledInConfig() &&
        ptr->getIntfType() != IntfType::WiFi) {
      mainIntf = ptr;
    }
    ptr = ptr->nextNetIntf;
  }
  if (mainIntf != nullptr) {
    netIntf = mainIntf;
  }
}

void Network::onLoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }

  uint8_t intfDisabled = 0;
  switch (getIntfType()) {
    case IntfType::WiFi: {
      cfg->getUInt8(Supla::WifiDisableTag, &intfDisabled);
      break;
    }
    case IntfType::Ethernet: {
      cfg->getUInt8(Supla::EthDisableTag, &intfDisabled);
      break;
    }
    case IntfType::Unknown: {
      break;
    }
  }
  if (intfDisabled) {
    SUPLA_LOG_INFO("[%s] network interface disabled", getIntfName());
    intfDisabledInConfig = true;
  } else {
    intfDisabledInConfig = false;
  }
}

Network::Network(unsigned char *ip) : Network() {
  if (ip == nullptr) {
    useLocalIp = false;
  } else {
    useLocalIp = true;
    memcpy(localIp, ip, 4);
  }
}

Network::Network() {
  mode = DEVICE_MODE_NORMAL;
  setSSLEnabled(true);
  if (netIntf == nullptr) {
    // first created interface is the default one
    netIntf = this;
  }
  if (firstNetIntf != nullptr) {
    auto ptr = firstNetIntf;
    while (ptr->nextNetIntf != nullptr) {
      ptr = ptr->nextNetIntf;
    }
    ptr->nextNetIntf = this;
  } else {
    firstNetIntf = this;
  }
}

Network::~Network() {
  if (firstNetIntf == this) {
    firstNetIntf = nextNetIntf;
  } else {
    auto ptr = firstNetIntf->nextNetIntf;
    auto prev = ptr;
    while (ptr && ptr != this) {
      prev = ptr;
      ptr = ptr->nextNetIntf;
    }
    if (ptr && prev) {
      prev->nextNetIntf = ptr->nextNetIntf;
    }
  }
  if (netIntf == this) {
    netIntf = firstNetIntf;
  }
}

Supla::Client *Network::createClient() {
  return Supla::ClientBuilder();
}

bool Network::iterate() {
  return false;
}

void Network::clearTimeCounters() {
}

void Network::fillStateData(TDSC_ChannelState *channelState) {
  (void)(channelState);
  SUPLA_LOG_DEBUG("[%s] fillStateData is not implemented for this interface",
      getIntfName());
}

#ifdef ARDUINO
#define TMP_STRING_SIZE 1024
#else
#define TMP_STRING_SIZE 2048
#endif

void Network::printData(const char *prefix, const void *buf, const int count) {
  char tmp[TMP_STRING_SIZE] = {};
  for (int i = 0; i < count && ((i + 1) * 3 < TMP_STRING_SIZE - 1); i++) {
    snprintf(
        tmp + i * 3, 4,
        "%02X ",
        static_cast<unsigned int>(static_cast<const unsigned char *>(buf)[i]));
  }
  SUPLA_LOG_VERBOSE("%s: [%s]", prefix, tmp);
}

void Network::setSsid(const char *wifiSsid) {
  (void)(wifiSsid);
}

void Network::setPassword(const char *wifiPassword) {
  (void)(wifiPassword);
}

void Network::setConfigMode() {
  mode = Supla::DEVICE_MODE_CONFIG;
  setupNeeded = true;
}

void Network::setNormalMode() {
  mode = Supla::DEVICE_MODE_NORMAL;
  setupNeeded = true;
}

void Network::uninit() {
}

bool Network::getMacAddr(uint8_t *buf) {
  (void)(buf);
  return false;
}

void Network::generateHostname(const char *prefix, int macSize, char *output) {
  const int hostnameSize = 32;
  char result[hostnameSize] = {};
  if (macSize > 6) {
    macSize = 6;
  }
  if (macSize < 0) {
    macSize = 0;
  }
  strncpy(result, prefix, hostnameSize);
  int destIdx = strnlen(result, hostnameSize);

  if (macSize > 0) {
    uint8_t mac[6] = {};
    getMacAddr(mac);
    if (result[destIdx - 1] != '-') {
      result[destIdx++] = '-';
    }
    destIdx +=
      generateHexString(mac + (6 - macSize), &(result[destIdx]), macSize);
  }
  memcpy(output, result, hostnameSize);
}

void Network::setHostname(const char *buf, int macSize) {
  generateHostname(buf, macSize, hostname);
  SUPLA_LOG_DEBUG("[%s] Network AP/hostname: %s", getIntfName(), hostname);
}

const char* Network::getIntfName() const {
  return "NA";
}


void Network::setSuplaDeviceClass(SuplaDeviceClass *ptr) {
  sdc = ptr;
}

bool Network::isWifiConfigRequired() {
  return false;
}

void Network::setSSLEnabled(bool enabled) {
  Supla::Protocol::SuplaSrpc::isSuplaSSLEnabled = enabled;
}

bool Network::popSetupNeeded() {
  if (setupNeeded) {
    setupNeeded = false;
    return true;
  }
  return false;
}

void Network::setSetupNeeded() {
  if (isIntfDisabledInConfig()) {
    return;
  }
  setupNeeded = true;
}

bool Network::isIpSetupTimeout() {
  return false;
}

enum Network::IntfType Network::getIntfType() const {
  return intfType;
}

uint32_t Network::getIP() {
  return 0;
}

bool Network::isIntfDisabledInConfig() const {
  if (mode == Supla::DEVICE_MODE_CONFIG && getIntfType() == IntfType::WiFi) {
    // If device is in config mode we alaways enable WiFi
    return false;
  }
  return intfDisabledInConfig;
}

};  // namespace Supla
