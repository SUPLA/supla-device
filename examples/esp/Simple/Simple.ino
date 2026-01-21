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
 * @file Simple.ino
 * @brief A simple example of a SUPLA device on ESP8266/ESP32.
 * This example demonstrates the basic structure of a SUPLA device firmware for ESP8266/ESP32 platforms.
 * It initializes the device, sets up a configuration button, a status LED, and a web server for Wi-Fi and SUPLA server configuration.
 * Network settings are configured via the web interface.
 * This example does not control any specific channels but serves as a starting point for building more complex devices.
 * It shows how to use the ESPWifi, LittleFsConfig, StatusLed, EspWebServer, and Button components.
 *
 * @tags simple, basic, esp, wifi, template
 */


#define BUTTON_CFG_GPIO 0
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
#include <supla/control/button.h>
#include <supla/storage/eeprom.h>

Supla::Eeprom eeprom;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  auto buttonCfg = new Supla::Control::Button(BUTTON_CFG_GPIO, true, true);
  buttonCfg->configureAsConfigButton(&SuplaDevice);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
