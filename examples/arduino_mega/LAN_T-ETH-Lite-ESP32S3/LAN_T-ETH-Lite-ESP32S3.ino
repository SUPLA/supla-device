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
#include <supla/sensor/impulse_counter.h>
#include <supla/device/supla_ca_cert.h>

// Choose where Supla should store counter data in persistant memory
// We recommend to use external FRAM memory
#define STORAGE_OFFSET 100
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom(STORAGE_OFFSET);
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

#include <supla/network/esp32ethspi.h>
/*
  Supla::ESPETHSPI Eth( type, phy_addr, cs_gpio, irq_gpio, rst_gpio, spi_host_device_t, sck_gpio, miso_gpio, mosi_gpio);
                         \/                                                 \/
                    --  type  --                                     ---  spi_host  ---
                   ETH_PHY_W5500                                  SPI2_HOST // with ESP32
                   ETH_PHY_DM9051                             SPI3_HOST // with ESP32-S2 and newer
                   ETH_PHY_KSZ8851
*/

Supla::ESPETHSPI Eth( ETH_PHY_W5500, 1, 9, 13, 14, SPI3_HOST, 10, 11, 12);  // example T-ETH-Lite-ESP32S3


void setup() {
  Serial.begin(115200);

  // Replace the falowing GUID with value that you can retrieve from
  // https://www.supla.org/arduino/get-guid
  char GUID[SUPLA_GUID_SIZE] = {0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00
                               };

  // Replace the following AUTHKEY with value that you can retrieve from:
  // https://www.supla.org/arduino/get-authkey
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00
                                     };

  /*
     Having your device already registered at cloud.supla.org,
     you want to change CHANNEL sequence or remove any of them,
     then you must also remove the device itself from cloud.supla.org.
     Otherwise you will get "Channel conflict!" error.
  */

  // CHANNEL0 - Impulse Counter on pin 34, counting raising edge (from LOW to
  // HIGH), no pullup on pin, and 10 ms debounce timeout
  auto ic1 = new Supla::Sensor::ImpulseCounter(34, true, false, 10);

  // CHANNEL1 - Impulse Counter on pin 35, counting falling edge (from HIGH to
  // LOW), with pullup on pin, and 50 ms debounce timeout
  auto ic2 = new Supla::Sensor::ImpulseCounter(35, false, true, 50);

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);

  /*
     Server address is available at https://cloud.supla.org
     If you do not have an account, you can create it at
     https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
     charge
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
