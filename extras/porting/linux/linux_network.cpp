// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  isDeviceReady = false;
}

bool Supla::LinuxNetwork::iterate() {
  return true;
}

void Supla::LinuxNetwork::fillStateData(TDSC_ChannelState *channelState) {
  // Source IP address will be configured by SuplaSrpc class
  // TODO(klew): add obtaining MAC address based on IP
  (void)(channelState);
}
