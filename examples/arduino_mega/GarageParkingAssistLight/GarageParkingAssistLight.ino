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

#include <SuplaDevice.h>
#include <supla/sensor/HC_SR04.h>
#include <supla/control/relay.h>
#include <supla/condition.h>
#include <supla/actions.h>

// Choose proper network interface for your card:
// Arduino Mega with EthernetShield W5100:
#include <supla/network/ethernet_shield.h>
// Ethernet MAC address
uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
Supla::EthernetShield ethernet(mac);

// Arduino Mega with ENC28J60:
// #include <supla/network/ENC28J60.h>
// Supla::ENC28J60 ethernet(mac);

void setup() {
  Serial.begin(115200);

  // Replace the falowing GUID with value that you can retrieve from
  // https://www.supla.org/arduino/get-guid
  char GUID[SUPLA_GUID_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Replace the following AUTHKEY with value that you can retrieve from:
  // https://www.supla.org/arduino/get-authkey
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00};

  /*
   * Having your device already registered at cloud.supla.org,
   * you want to change CHANNEL sequence or remove any of them,
   * then you must also remove the device itself from cloud.supla.org.
   * Otherwise you will get "Channel conflict!" error.
   */

  auto distance = new Supla::Sensor::HC_SR04(12,13);//(trigPin, echoPin)

  auto redLight = new Supla::Control::Relay(22);
  auto orangeLight = new Supla::Control::Relay(23);
  auto greenLight = new Supla::Control::Relay(24);

  distance->addAction(Supla::TURN_ON, redLight, OnLess(0.40));
  distance->addAction(Supla::TURN_ON, orangeLight, OnBetween(0.40, 0.80));
  distance->addAction(Supla::TURN_ON, greenLight, OnBetween(0.80, 1.50));

  distance->addAction(Supla::TURN_OFF, orangeLight, OnLess(0.35));
  distance->addAction(Supla::TURN_OFF, greenLight, OnLess(0.75));

  distance->addAction(Supla::TURN_OFF, redLight, OnGreater(0.45));
  distance->addAction(Supla::TURN_OFF, orangeLight, OnGreater(0.85));
  distance->addAction(Supla::TURN_OFF, greenLight, OnGreater(1.55));

  /*
   * SuplaDevice Initialization.
   * Server address is available at https://cloud.supla.org
   * If you do not have an account, you can create it at
   * https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
   * charge
   */

  SuplaDevice.begin(
      GUID,              // Global Unique Identifier
      "svr1.supla.org",  // SUPLA server address
      "email@address",   // Email address used to login to Supla Cloud
      AUTHKEY);          // Authorization key
}

void loop() {
  SuplaDevice.iterate();
}

