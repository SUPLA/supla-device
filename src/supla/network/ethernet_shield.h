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

#ifndef SRC_SUPLA_NETWORK_ETHERNET_SHIELD_H_
#define SRC_SUPLA_NETWORK_ETHERNET_SHIELD_H_

#include <Arduino.h>
#include <Ethernet.h>

#include <supla/log_wrapper.h>

#include "../supla_lib_config.h"
#include "network.h"

// TODO(klew): change logs to supla_log

namespace Supla {
class EthernetShield : public Supla::Network {
 public:
  explicit EthernetShield(uint8_t mac[6], unsigned char *ip = NULL)
      : Network(ip), isDeviceReady(false) {
    memcpy(this->mac, mac, 6);
    sslEnabled = false;  // SSL is not supported on Arduino MEGA target
  }

  int read(void *buf, int count) {
    _supla_int_t size = client.available();
    if (size > 0) {
      if (size > count) size = count;
      int readSize = client.read(reinterpret_cast<uint8_t *>(buf), size);
#ifdef SUPLA_COMM_DEBUG
      Serial.print(F("Received: ["));
      for (int i = 0; i < readSize; i++) {
        Serial.print(static_cast<unsigned char *>(buf)[i], HEX);
        Serial.print(F(" "));
      }
      Serial.println(F("]"));
#endif
      return readSize;
    }

    return -1;
  }

  int write(void *buf, int count) {
#ifdef SUPLA_COMM_DEBUG
    Serial.print(F("Sending: ["));
    for (int i = 0; i < count; i++) {
      Serial.print(static_cast<unsigned char *>(buf)[i], HEX);
      Serial.print(F(" "));
    }
    Serial.println(F("]"));
#endif
    int sendSize = client.write(reinterpret_cast<const uint8_t *>(buf), count);
    return sendSize;
  }

  int connect(const char *server, int port = -1) {
    int connectionPort = (port == -1 ? 2015 : port);
    SUPLA_LOG_DEBUG(
              "Establishing connection with: %s (port: %d)",
              server,
              connectionPort);

    return client.connect(server, connectionPort);
  }

  bool connected() {
    return client.connected();
  }

  void disconnect() {
    client.stop();
  }

  bool isReady() {
    return isDeviceReady;
  }

  void setup() {
    Serial.println(F("Connecting to network..."));
    if (useLocalIp) {
      Ethernet.begin(mac, localIp);
      isDeviceReady = true;
    } else {
      int result = false;
      result = Ethernet.begin(mac, 10000, 4000);
      Serial.print(F("DHCP connection result: "));
      Serial.println(result);
      isDeviceReady = result == 1 ? true : false;
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

  bool iterate() {
    Ethernet.maintain();
    return true;
  }

  void fillStateData(TDSC_ChannelState *channelState) {
    channelState->Fields |=
        SUPLA_CHANNELSTATE_FIELD_IPV4 | SUPLA_CHANNELSTATE_FIELD_MAC;
    channelState->IPv4 = Ethernet.localIP();
    Ethernet.MACAddress(channelState->MAC);
  }

  void setSSLEnabled(bool enabled) override {
    (void)(enabled);
  };

 protected:
  EthernetClient client;
  uint8_t mac[6];
  bool isDeviceReady;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ETHERNET_SHIELD_H_
