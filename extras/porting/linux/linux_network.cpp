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
#include <supla/supla_lib_config.h>
#include <sys/signal.h>
#include <supla/log_wrapper.h>

#include "linux_network.h"

Supla::LinuxNetwork::LinuxNetwork() : Network(nullptr) {
  signal(SIGPIPE, SIG_IGN);
}

Supla::LinuxNetwork::~LinuxNetwork() {
  DisconnectProtocols();
}

bool Supla::LinuxNetwork::isReady() {
  return isDeviceReady;
}

void Supla::LinuxNetwork::setup() {
  isDeviceReady = true;
}

void Supla::LinuxNetwork::disable() {
}

bool Supla::LinuxNetwork::iterate() {
  return true;
}

void Supla::LinuxNetwork::fillStateData(TDSC_ChannelState *channelState) {
  channelState->Fields |= SUPLA_CHANNELSTATE_FIELD_IPV4;

  // TODO(klew): add obtaining local ip address for channel state info
/*
  struct sockaddr_in address = {};
  socklen_t len = sizeof(address);
  getsockname(connectionFd, (struct sockaddr *)&address, &len);
  channelState->IPv4 =
      static_cast<unsigned _supla_int_t>(address.sin_addr.s_addr);
*/
}
