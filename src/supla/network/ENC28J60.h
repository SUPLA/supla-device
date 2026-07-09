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

#ifndef SRC_SUPLA_NETWORK_ENC28J60_H_
#define SRC_SUPLA_NETWORK_ENC28J60_H_

#ifdef ARDUINO_ARCH_AVR
#include <Arduino.h>
#include <EthernetENC.h>

#include <supla/log_wrapper.h>

#include "../supla_lib_config.h"
#include "network.h"

namespace Supla {
class ENC28J60 : public Supla::Network {
 public:
  explicit ENC28J60(uint8_t mac[6], unsigned char *ip = NULL) : Network(ip) {
    memcpy(this->mac, mac, 6);
  }

  void disable() override {
    isDeviceReady = false;
  }

  bool isReady() override {
    return isDeviceReady;
  }

  void setup() override {
    setSSLEnabled(false);  // no SSL support on Arduino MEGA target
    SUPLA_LOG_INFO("Connecting to network...");
    if (useLocalIp) {
      Ethernet.begin(mac, localIp);
      isDeviceReady = true;
    } else {
      isDeviceReady = Ethernet.begin(mac) == 1;
    }

    IPAddress localIP = Ethernet.localIP();
    IPAddress subnetMaskIP = Ethernet.subnetMask();
    IPAddress gatewayIP = Ethernet.gatewayIP();
    IPAddress dnsServerIP = Ethernet.dnsServerIP();
    SUPLA_LOG_INFO("localIP: %d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
    SUPLA_LOG_INFO("subnetMaskIP: %d.%d.%d.%d", subnetMaskIP[0], subnetMaskIP[1], subnetMaskIP[2], subnetMaskIP[3]);
    SUPLA_LOG_INFO("gatewayIP: %d.%d.%d.%d", gatewayIP[0], gatewayIP[1], gatewayIP[2], gatewayIP[3]);
    SUPLA_LOG_INFO("dnsServerIP: %d.%d.%d.%d", dnsServerIP[0], dnsServerIP[1], dnsServerIP[2], dnsServerIP[3]);
  }

 protected:
  uint8_t mac[6] = {};
  bool isDeviceReady = false;
};

};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR

#endif  // SRC_SUPLA_NETWORK_ENC28J60_H_
