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

/**
 * @supla-example
 * @file LAN_T-ETH-Lite-ESP32S3.ino
 * @brief Example of implementing impulse counters on a T-ETH-Lite-ESP32S3 board with Ethernet connectivity for SUPLA.
 * This example demonstrates how to configure two impulse counters on different pins (counting rising or falling edges, with debounce)
 * on an ESP32S3 board with a W5500 Ethernet PHY.
 * The device integrates with the SUPLA cloud via Ethernet and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * It can store counter data persistently in EEPROM (or optionally FRAM).
 * Users need to adjust GPIO pins for the impulse counters and Ethernet configuration in the code.
 * A status LED is also configured.
 *
 * @tags impulse, counter, ESP32S3, W5500, Ethernet, LAN, T-ETH-Lite, EEPROM, FRAM, esp, web_interface
 */
#include <SuplaDevice.h>
#include <supla/sensor/impulse_counter.h>
#include <supla/network/esp32ethspi.h>
#include <supla/storage/littlefs_config.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>

// Choose where Supla should store state data in persistent memory
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

#define STATUS_LED_GPIO 2

/*
  Supla::ESPETHSPI Eth( type, phy_addr, cs_gpio, irq_gpio, rst_gpio, spi_host_device_t, sck_gpio, miso_gpio, mosi_gpio);
                         \/                                                 \/
                    --  type  --                                     ---  spi_host  ---
                   ETH_PHY_W5500                                  SPI2_HOST // with ESP32
                   ETH_PHY_DM9051                             SPI3_HOST // with ESP32-S2 and newer
                   ETH_PHY_KSZ8851
*/

Supla::ESPETHSPI Eth( ETH_PHY_W5500, 1, 9, 13, 14, SPI3_HOST, 10, 11, 12);  // example T-ETH-Lite-ESP32S3
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // CHANNEL0 - Impulse Counter on pin 34, counting raising edge (from LOW to
  // HIGH), no pullup on pin, and 10 ms debounce timeout
  auto ic1 = new Supla::Sensor::ImpulseCounter(34, true, false, 10);

  // CHANNEL1 - Impulse Counter on pin 35, counting falling edge (from HIGH to
  // LOW), with pullup on pin, and 50 ms debounce timeout
  auto ic2 = new Supla::Sensor::ImpulseCounter(35, false, true, 50);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
