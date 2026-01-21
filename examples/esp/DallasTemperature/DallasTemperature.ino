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
 * @file DallasTemperature.ino
 * @brief Example of connecting multiple DS18B20 temperature sensors to SUPLA using an ESP8266/ESP32.
 * This example configures an ESP device with Wi-Fi to read data from multiple DS18B20 sensors (on a single pin)
 * and integrate it with the SUPLA cloud. It includes a web server for Wi-Fi and SUPLA server configuration.
 * Network settings are configured via the web interface.
 * Users may need to adjust the addresses of their DS18B20 sensors in the code.
 * A status LED is also configured.
 *
 * @dependency https://github.com/milesburton/Arduino-Temperature-Control-Library
 * @tags DS18B20, DallasTemperature, temperature, sensor, onewire, esp, esp32, esp8266, wifi, web_interface
 */

#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/DS18B20.h>
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

  // CHANNEL0-3 - Thermometer DS18B20
  // 4 DS18B20 thermometers at pin 23. DS address can be omitted when there is
  // only one device at a pin
  //
  // TODO(anyone): add HTML elements for DS18B20 configuration (scan, etc)
  DeviceAddress ds1addr = {0x28, 0xFF, 0xC8, 0xAB, 0x6E, 0x18, 0x01, 0xFC};
  DeviceAddress ds2addr = {0x28, 0xFF, 0x54, 0x73, 0x6E, 0x18, 0x01, 0x77};
  DeviceAddress ds3addr = {0x28, 0xFF, 0x55, 0xCA, 0x6B, 0x18, 0x01, 0x8D};
  DeviceAddress ds4addr = {0x28, 0xFF, 0x4F, 0xAB, 0x6E, 0x18, 0x01, 0x66};

  new Supla::Sensor::DS18B20(23, ds1addr);
  new Supla::Sensor::DS18B20(23, ds2addr);
  new Supla::Sensor::DS18B20(23, ds3addr);
  new Supla::Sensor::DS18B20(23, ds4addr);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
