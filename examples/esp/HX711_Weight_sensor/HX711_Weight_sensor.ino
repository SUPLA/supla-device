// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file HX711_Weight_sensor.ino
 * @brief Example of connecting an HX711 weight sensor to SUPLA with an ESP8266/ESP32.
 * This example demonstrates how to integrate an HX711 load cell amplifier with a weight sensor
 * to an ESP device, providing weight measurements to the SUPLA cloud via Wi-Fi.
 * It includes functionality for taring the scales using an external button and a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the HX711 module and the button in the code.
 * A status LED is also configured.
 *
 * @dependency https://github.com/olkal/HX711_ADC
 * @tags HX711, weight, load_cell, sensor, scales, tare, button, esp, esp32, esp8266, wifi, web_interface
 */

#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/HX711.h>
#include <supla/storage/littlefs_config.h>

#define SDA_GPIO        21
#define SCL_GPIO        22
#define BUTTON_GPIO     4
#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

// Choose where Supla should store HX711 configuration data in persistent memory
// We recommend to use external FRAM memory
// #define FRAM_CS_PIN TBD // choose a free GPIO and replace TBD
// #define STORAGE_OFFSET 100
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(FRAM_CS_PIN, STORAGE_OFFSET);
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  auto scales = new Supla::Sensor::HX711(SDA_GPIO, SCL_GPIO);
  // Optionally, enter the calibration value as the third parameter (SDA_GPIO,
  // SCL_GPIO, 696.0)
  auto button = new Supla::Control::Button(BUTTON_GPIO);
  button->setSwNoiseFilterDelay(50);
  button->addAction(Supla::TARE_SCALES, scales, Supla::ON_PRESS);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
