// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file HC_SR04_Distance_sensor.ino
 * @brief Basic example of an HC-SR04 ultrasonic distance sensor with an ESP8266/ESP32 for SUPLA integration.
 * This example demonstrates how to connect a standard HC-SR04 ultrasonic sensor to an ESP device
 * and integrate its distance readings with the SUPLA cloud via Wi-Fi.
 * It includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the sensor in the code.
 * A status LED is also configured.
 *
 * @tags HC-SR04, ultrasonic, distance, sensor, esp, esp32, esp8266, wifi, web_interface
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

  new Supla::Sensor::HC_SR04(12, 13);  // (trigPin, echoPin)

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
