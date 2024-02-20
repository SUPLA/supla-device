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

void Network::DisconnectProtocols() {
  for (auto proto = Supla::Protocol::ProtocolLayer::first(); proto != nullptr;
      proto = proto->next()) {
    proto->disconnect();
  }
}

void Network::Setup() {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->setup();
    ptr = ptr->nextNetIntf;
  }
}

void Network::Disable() {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->disable();
    ptr = ptr->nextNetIntf;
  }
}

void Network::Uninit() {
  auto ptr = firstNetIntf;
  while (ptr) {
    ptr->uninit();
    ptr = ptr->nextNetIntf;
  }
}

bool Network::IsReady() {
  if (Instance() != nullptr) {
    return Instance()->isReady();
  }
  return false;
}

bool Network::Iterate() {
  bool result = false;
  auto ptr = firstNetIntf;
  while (ptr) {
    if (ptr->iterate()) {
      result = true;
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
    ptr->setSetupNeeded();
    ptr = ptr->nextNetIntf;
  }
}

bool Network::PopSetupNeeded() {
  bool setupNeeded = false;
  auto ptr = firstNetIntf;
  while (ptr) {
    if (ptr->popSetupNeeded()) {
      setupNeeded = true;
    }
    ptr = ptr->nextNetIntf;
  }
  return setupNeeded;
}

bool Network::GetMacAddr(uint8_t *buf) {
  if (Instance() != nullptr) {
    return Instance()->getMacAddr(buf);
  }
  return false;
}

void Network::SetHostname(const char *buf, int macSize) {
  auto ptr = firstNetIntf;
  if (macSize < 0) {
    macSize = 0;
  }
  if (macSize > 6) {
    macSize = 6;
  }
  while (ptr) {
    ptr->setHostname(buf, macSize);
    ptr = ptr->nextNetIntf;
  }
}

bool Network::IsSuplaSSLEnabled() {
  if (Instance() != nullptr) {
    return Instance()->isSuplaSSLEnabled();
  }
  return false;
}

bool Network::IsIpSetupTimeout() {
  if (Instance() != nullptr) {
    return Instance()->isIpSetupTimeout();
  }
  return false;
}

void Network::LoadConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }

  uint8_t netIntfType = 255;
  cfg->getUInt8(Supla::NetIntfTypeTag, &netIntfType);
  enum IntfType selectedIntfType = IntfType::WiFi;
  switch (netIntfType) {
    case 0: {
      SUPLA_LOG_INFO("Using WiFi as default network interface");
      selectedIntfType = IntfType::WiFi;
      break;
    }
    case 1: {
      SUPLA_LOG_INFO("Using Ethernet as default network interface");
      selectedIntfType = IntfType::Ethernet;
      break;
    }
  }

  auto ptr = firstNetIntf;
  while (ptr && ptr->getIntfType() != selectedIntfType) {
    ptr = ptr->nextNetIntf;
  }
  if (ptr) {
    netIntf = ptr;
  }
}

Network::Network(unsigned char *ip) {
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

  if (ip == nullptr) {
    useLocalIp = false;
  } else {
    useLocalIp = true;
    memcpy(localIp, ip, 4);
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

bool Network::iterate() {
  return false;
}

void Network::clearTimeCounters() {
}

void Network::fillStateData(TDSC_ChannelState *channelState) {
  (void)(channelState);
  SUPLA_LOG_DEBUG("fillStateData is not implemented for this interface");
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
  SUPLA_LOG_DEBUG("Network AP/hostname: %s", hostname);
}

void Network::setSuplaDeviceClass(SuplaDeviceClass *ptr) {
  sdc = ptr;
}

bool Network::isWifiConfigRequired() {
  return false;
}

void Network::setSSLEnabled(bool enabled) {
  sslEnabled = enabled;
}

void Network::setCACert(const char *rootCA) {
  rootCACert = rootCA;
}

bool Network::popSetupNeeded() {
  if (setupNeeded) {
    setupNeeded = false;
    return true;
  }
  return false;
}

void Network::setSetupNeeded() {
  setupNeeded = true;
}

bool Network::isSuplaSSLEnabled() {
  return sslEnabled;
}

bool Network::isIpSetupTimeout() {
  return false;
}

enum Network::IntfType Network::getIntfType() const {
  return intfType;
}

};  // namespace Supla
