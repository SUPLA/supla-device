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

/*
 * DHT sensor example for ESP8266 or ESP32.
 * This example requires DHT sensor library installed.
 * https://github.com/adafruit/DHT-sensor-library
 *
 * Please adjust GPIO pin numbers according to your setup.
 */


#include <SuplaDevice.h>
#include <supla/sensor/DHT.h>
#include <supla/network/esp_wifi.h>
#include <supla/storage/littlefs_config.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>

#define DHT1PIN  4
#define DHT1TYPE DHT22
#define DHT2PIN  5
#define DHT2TYPE DHT22
#define STATUS_LED_GPIO 2

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

  // This example adds two DHT22 sensors:
  // CHANNEL0 - DHT22 Sensor
  new Supla::Sensor::DHT(DHT1PIN, DHT1TYPE);

  // CHANNEL1 - DHT22 Sensor
  new Supla::Sensor::DHT(DHT2PIN, DHT2TYPE);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}

