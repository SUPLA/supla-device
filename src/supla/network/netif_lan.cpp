/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "netif_lan.h"

Supla::LAN::LAN() {
  intfType = IntfType::Ethernet;
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
