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
 * @file Pzem_V_2.ino
 * @brief Example of connecting a Peacefair PZEM-004T V2 energy monitor to SUPLA using an ESP8266/ESP32.
 * This example demonstrates how to integrate a PZEM-004T V2 energy monitor with an ESP device
 * and send its readings (voltage, current, power, energy) to the SUPLA cloud via Wi-Fi.
 * It includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust the RX/TX pins for the PZEM module in the code.
 * A status LED is also configured.
 *
 * @dependency https://github.com/olehs/PZEM004T
 * @tags PZEM, PZEM-004T, V2, energy, monitor, voltage, current, power, esp, esp32, esp8266, wifi, web_interface
 */

#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/PzemV2.h>
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

  new Supla::Sensor::PZEMv2(5, 4);  // (RX,TX)

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
