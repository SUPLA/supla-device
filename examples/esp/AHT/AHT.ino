// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file AHT.ino
 * @author SUPLA
 * @brief Example of connecting an AHT temperature and humidity sensor to SUPLA using an ESP8266/ESP32.
 * This example configures an ESP device with Wi-Fi to read data from an AHT sensor and integrate it with the SUPLA cloud.
 * It includes a web server for Wi-Fi and SUPLA server configuration.
 * It requires the Adafruit AHT sensor library to be installed.
 * Users need to adjust network settings. A status LED is also configured.
 *
 * @tags AHT, temperature, humidity, sensor, esp, esp32, esp8266, wifi
 */


#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/AHT.h>
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

  new Supla::Sensor::AHT();

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
