// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "netif_lan.h"

#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

Supla::LAN::LAN() {
  intfType = IntfType::Ethernet;
}

void Supla::LAN::onLoadConfig() {
  Network::onLoadConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  cfg->loadNetifConfig(Supla::ConfigTag::EthNetifCfgTag, &netifConfig);
}

const char* Supla::LAN::getIntfName() const {
  return "Ethernet";
}

bool Supla::LAN::isReady() {
  return isIpReady;
}

void Supla::LAN::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4 |
    SUPLA_CHANNELSTATE_FIELD_MAC;

  getMacAddr(channelState->MAC);
  channelState->IPv4 = ipv4;
}

void Supla::LAN::setIpReady(bool ready) {
  isIpReady = ready;
}

void Supla::LAN::setIpv4Addr(uint32_t ip) {
  ipv4 = ip;
  if (ip == 0) {
    setIpReady(false);
  } else {
    setIpReady(true);
  }
}

uint32_t Supla::LAN::getIP() {
  if (isReady()) {
    return ipv4;
  }
  return 0;
}
