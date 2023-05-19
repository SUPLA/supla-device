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

#include <Arduino.h>
#include <EthernetENC.h>

#include <supla/log_wrapper.h>

#include "../supla_lib_config.h"
#include "network.h"

// TODO(klew): change logs to supla_log

namespace Supla {
class ENC28J60 : public Supla::Network {
 public:
  explicit ENC28J60(uint8_t mac[6], unsigned char *ip = NULL) : Network(ip) {
    memcpy(this->mac, mac, 6);
    sslEnabled = false;
  }

  void disable() override {
  }

  bool isReady() override {
    return true;
  }

  void setup() override {
    Serial.println(F("Connecting to network..."));
    if (useLocalIp) {
      Ethernet.begin(mac, localIp);
    } else {
      Ethernet.begin(mac);
    }

    Serial.print(F("localIP: "));
    Serial.println(Ethernet.localIP());
    Serial.print(F("subnetMask: "));
    Serial.println(Ethernet.subnetMask());
    Serial.print(F("gatewayIP: "));
    Serial.println(Ethernet.gatewayIP());
    Serial.print(F("dnsServerIP: "));
    Serial.println(Ethernet.dnsServerIP());
  }

  void setSSLEnabled(bool enabled) override {
    (void)(enabled);
  };

 protected:
  uint8_t mac[6] = {};
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ENC28J60_H_
