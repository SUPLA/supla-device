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
 * @file HC_SR04_Distance_sensor_extended.ino
 * @brief Extended example of an HC-SR04 ultrasonic distance sensor with custom mapping for display in SUPLA, using an ESP8266/ESP32.
 * This example demonstrates how to connect an HC-SR04 ultrasonic sensor to an ESP device
 * and integrate its distance readings with the SUPLA cloud via Wi-Fi.
 * It includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the sensor, as well as the distance mapping parameters in the code.
 * A status LED is also configured.
 *
 * @tags HC-SR04, ultrasonic, distance, sensor, extended, mapping, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/HC_SR04.h>
#include <supla/storage/littlefs_config.h>
#include <stdint.h>

#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

int8_t trigPin = 13;
int8_t echoPin = 12;
int16_t minSensorRead = 5;   // minimum sensor reading distance in centimeters
int16_t maxSensorRead = 55;  // maximum sensor reading distance in centimeters
int16_t minAppReading = 50;  // minimum distance shown by the App in centimeters
                             // when sensor read <= min_sensor_read
int16_t maxAppReading = 0;  // maximum distance shown by the App in centimeters
                            // when sensor read >= max_sensor_read

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  new Supla::Sensor::HC_SR04(trigPin,
                             echoPin,
                             minSensorRead,
                             maxSensorRead,
                             minAppReading,
                             maxAppReading);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
