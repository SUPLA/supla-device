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
 * @file GarageParkingAssistLight.ino
 * @brief Example of a garage parking assist light system using an HC-SR04 distance sensor with an ESP8266/ESP32.
 * This example demonstrates how to implement a parking assist system that uses an HC-SR04 ultrasonic sensor
 * to measure the distance to a car and controls three relays (for red, orange, and green lights) based on predefined distance thresholds.
 * It integrates with the SUPLA cloud via Wi-Fi and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the sensor and relays in the code.
 * A status LED is also configured.
 *
 * @tags garage, parking, assist, light, HC-SR04, ultrasonic, distance, sensor, relay, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/actions.h>
#include <supla/condition.h>
#include <supla/control/relay.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/HC_SR04.h>
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

  auto distance = new Supla::Sensor::HC_SR04(12, 13);  // (trigPin, echoPin)

  auto redLight = new Supla::Control::Relay(22);
  auto orangeLight = new Supla::Control::Relay(23);
  auto greenLight = new Supla::Control::Relay(24);

  distance->addAction(Supla::TURN_ON, redLight, OnLess(0.40));
  distance->addAction(Supla::TURN_ON, orangeLight, OnBetween(0.40, 0.80));
  distance->addAction(Supla::TURN_ON, greenLight, OnBetween(0.80, 1.50));

  distance->addAction(Supla::TURN_OFF, orangeLight, OnLess(0.35));
  distance->addAction(Supla::TURN_OFF, greenLight, OnLess(0.75));

  distance->addAction(Supla::TURN_OFF, redLight, OnGreater(0.45));
  distance->addAction(Supla::TURN_OFF, orangeLight, OnGreater(0.85));
  distance->addAction(Supla::TURN_OFF, greenLight, OnGreater(1.55));

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
