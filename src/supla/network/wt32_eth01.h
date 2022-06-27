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

/*  - for WT32-ETH01 - */

#ifndef SRC_SUPLA_NETWORK_WT32_ETH01_H_
#define SRC_SUPLA_NETWORK_WT32_ETH01_H_

#include <Arduino.h>
#include <ETH.h>
#include <WiFiClientSecure.h>
#include <supla/network/network.h>
#include <supla/supla_lib_config.h>

#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

// Pin# of the enable signal for the external crystal oscillator (-1 to disable
// for internal APLL source)
#define ETH_POWER_PIN 16

// Type of the Ethernet PHY (LAN8720 or TLK110)
#define ETH_TYPE ETH_PHY_LAN8720

// I²C-address of Ethernet PHY (0 or 1 for LAN8720, 31 for TLK110)
#define ETH_ADDR 1

// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDC_PIN 23

// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_MDIO_PIN 18

static bool eth_connected = false;

namespace Supla {
class WT32_ETH01 : public Supla::Network {
 public:
  explicit WT32_ETH01(uint8_t ethmode, unsigned char *ip = nullptr)
      : Network(ip), client(nullptr) {
    if (ethmode == 0) {
      ETH_ADDRESS = 0;
    } else {
      ETH_ADDRESS = 1;
    }
  }

  int read(void *buf, int count) {
    _supla_int_t size = client->available();
    if (size > 0) {
      if (size > count) size = count;
      int readSize = client->read(reinterpret_cast<uint8_t *>(buf), size);
#ifdef SUPLA_COMM_DEBUG
      Serial.print(F("Received: ["));
      for (int i = 0; i < readSize; i++) {
        Serial.print(static_cast<unsigned char *>(buf)[i], HEX);
        Serial.print(F(" "));
        delay(0);
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
      delay(0);
    }
    Serial.println(F("]"));
#endif
    int sendSize = client->write(reinterpret_cast<const uint8_t *>(buf), count);
    return sendSize;
  }

  int connect(const char *server, int port = -1) {
    String message;
    if (client) {
      delete client;
      client = nullptr;
    }
    if (sslEnabled) {
      message = "Secured connection";
      auto clientSec = new WiFiClientSecure();
      client = clientSec;
      if (caCert.length() > 0) {
        message += " with certificate matching";
        clientSec->setCACert(caCert.c_str());
      } else if (rootCACert) {
        clientSec->setCACert(rootCACert);
      } else {
        message += " without certificate matching";
        clientSec->setInsecure();
      }
    } else {
      message = "unsecured connection";
      client = new WiFiClient();
    }

    int connectionPort = (sslEnabled ? 2016 : 2015);
    if (port != -1) {
      connectionPort = port;
    }

    supla_log(LOG_DEBUG,
              "Establishing %s with: %s (port: %d)",
              message.c_str(),
              server,
              connectionPort);

    bool result = client->connect(server, connectionPort);
    return result;
  }

  bool connected() {
    return (client != NULL) && client->connected();
  }

  bool isReady() {
    return eth_connected;
  }

  void disconnect() {
    if (client != nullptr) {
      client->stop();
    }
  }

  void setup() {
    WiFiEventId_t event_gotIP = WiFi.onEvent(
        [](WiFiEvent_t event, WiFiEventInfo_t info) {
          Serial.print(F("local IP: "));
          Serial.println(ETH.localIP());
          Serial.print(F("subnetMask: "));
          Serial.println(ETH.subnetMask());
          Serial.print(F("gatewayIP: "));
          Serial.println(ETH.gatewayIP());
          Serial.print(F("ETH MAC: "));
          Serial.println(ETH.macAddress());
          if (ETH.fullDuplex()) {
            Serial.print(F("FULL_DUPLEX , "));
          }
          Serial.print(ETH.linkSpeed());
          Serial.println(F("Mbps"));
          eth_connected = true;
        },
        WiFiEvent_t::ARDUINO_EVENT_ETH_GOT_IP);  // ESP core 2.0.2
    (void)(event_gotIP);
    WiFiEventId_t event_disconnected = WiFi.onEvent(
        [](WiFiEvent_t event, WiFiEventInfo_t info) {
          Serial.println(F("Station disconnected"));
          eth_connected = false;
        },
        WiFiEvent_t::ARDUINO_EVENT_ETH_DISCONNECTED);  // ESP core 2.0.2
    (void)(event_disconnected);
    Serial.println(F("establishing Lan connection"));
    ETH.begin(ETH_ADDRESS,
              ETH_POWER_PIN,
              ETH_MDC_PIN,
              ETH_MDIO_PIN,
              ETH_TYPE,
              ETH_CLK_MODE);
  }

  void setServersCACert(String value) {
    caCert = value;
  }

  void setTimeout(int timeoutMs) {
    if (client) {
      client->setTimeout(timeoutMs);
    }
  }

  void fillStateData(TDSC_ChannelState *channelState) {
    channelState->Fields |=
        SUPLA_CHANNELSTATE_FIELD_IPV4 | SUPLA_CHANNELSTATE_FIELD_MAC;
    channelState->IPv4 = ETH.localIP();
    ETH.macAddress(channelState->MAC);  // ESP core 2.0.2
  }

 protected:
  WiFiClient *client = NULL;
  uint8_t ETH_ADDRESS;
  String caCert;
};
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WT32_ETH01_H_
