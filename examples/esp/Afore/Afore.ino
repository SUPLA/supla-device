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
 * @file Afore.ino
 * @brief Example of connecting an Afore inverter to SUPLA using an ESP8266/ESP32.
 * This example configures an ESP device with Wi-Fi to communicate with an Afore photovoltaic inverter
 * and integrate its data with the SUPLA cloud. It includes a web server for Wi-Fi and SUPLA server configuration.
 * Network settings are configured via the web interface.
 * Users still need to adjust the Afore inverter's IP address, port, and credentials in the code.
 * A status LED is also configured.
 *
 * @tags afore, inverter, photovoltaic, esp, esp32, esp8266, wifi, pv, energy, web_interface
 */

#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/pv/afore.h>
#include <supla/storage/littlefs_config.h>

#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // CHANNEL0
  // Put IP address of your Afore inverter, then port, and last parametere is
  // base64 encoded "login:password" You can use any online base64 encoder to
  // convert your login and password, i.e. https://www.base64encode.org/
  //
  // TODO(anyone): add HTML element to configure Afore inverter in web interface
  new Supla::PV::Afore(IPAddress(192, 168, 0, 59), 80, "bG9naW46cGFzc3dvcmQ=");

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
