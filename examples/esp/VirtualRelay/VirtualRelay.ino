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
 * @file VirtualRelay.ino
 * @brief Example of a virtual relay for SUPLA using an ESP8266/ESP32.
 * This example demonstrates how to create a virtual relay that exists only in the SUPLA cloud,
 * without controlling any physical output. It can be toggled via the SUPLA app or web interface,
 * and its state can be restored after a device restart.
 * It uses Wi-Fi for network connectivity and includes a web server for configuration.
 * Users need to adjust network settings. A status LED is also configured.
 *
 * @tags virtual_relay, relay, esp, esp32, esp8266, wifi
 */

#define STATUS_LED_GPIO 2

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/control/virtual_relay.h>
#include <supla/storage/eeprom.h>

Supla::Eeprom eeprom;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Channel #0 Virtual Relay
  auto vr = new Supla::Control::VirtualRelay();
  vr->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);
  // Enable state restore after device restart:
  vr->setDefaultStateRestore();

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}

