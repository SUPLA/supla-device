/*
   Copyright (C) AC SOFTWARE SP. Z O.O.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
   */

/**
 * @supla-example
 * @file Pzem_V_3.ino
 * @brief Example of connecting a Peacefair PZEM-004T V3 energy monitor to SUPLA using an Arduino Mega.
 * This example demonstrates how to integrate a PZEM-004T V3 energy monitor with an Arduino Mega
 * and send its readings (voltage, current, power, energy) to the SUPLA cloud via an Ethernet shield (W5100 or ENC28J60).
 * Users need to adjust network settings, SUPLA GUID, AUTHKEY, and the RX/TX pins for the PZEM module, and optionally the PZEM address in the code.
 *
 * @dependency https://github.com/mandulaj/PZEM-004T-v30
 * @tags PZEM, PZEM-004T, V3, energy, monitor, voltage, current, power, arduino_mega, ethernet, w5100, enc28j60, modbus
 */

#include <SuplaDevice.h>
#include <supla/sensor/PzemV3.h>

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

  new Supla::Sensor::PZEMv3(5, 4);  // (RX,TX)  "PZEM Addr default to 0xF8"

  // new Supla::Sensor::PZEMv3(5, 4, 0x10);  // (RX,TX, PZEM Addr)

  /*
   * SuplaDevice Initialization.
   * Server address, is available at https://cloud.supla.org
   * If you do not have an account, you can create it at
   * https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
   * charge
   *
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
