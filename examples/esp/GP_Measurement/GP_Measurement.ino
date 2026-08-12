// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file GP_Measurement.ino
 * @brief Example of using a General Purpose Measurement (GPM) virtual sensor for SUPLA with an ESP8266/ESP32.
 * This example demonstrates the flexibility of the General Purpose Measurement (GPM) sensor type,
 * which acts as a virtual channel capable of displaying any double value.
 * It showcases how to set initial values, define custom units (before and after value),
 * adjust value precision, and apply multipliers/dividers for scaling.
 * The device integrates with the SUPLA cloud via Wi-Fi and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for their board configuration in the code.
 * A status LED is also configured.
 *
 * @tags GPM, GeneralPurposeMeasurement, virtual_sensor, custom_units, scaling, precision, esp, esp32, esp8266, wifi, web_interface
 */

// Please adjust GPIO to your board configuration
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
#include <supla/sensor/general_purpose_measurement.h>

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

Supla::Sensor::GeneralPurposeMeasurement *gpm = nullptr;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Channels configuration
  // Channel #0 General Purpose Measurement
  // GPM class by deafult works as "virtual channel" (it accepts any value
  // via setValue(double) method.
  gpm = new Supla::Sensor::GeneralPurposeMeasurement();

  // Default channel config values are initialized only once. They can be
  // modified later, but they won't change corresponding used configurable
  // parameters.
  // Below lines are optional, just remove them if you don't want to set
  // them.
  gpm->setDefaultValueDivider(0);  // in 0.001 steps, 0 is ignored
  gpm->setDefaultValueMultiplier(0);  // in 0.001 steps, 0 is ignored
  gpm->setDefaultValueAdded(0);  // in 0.001 steps
  gpm->setDefaultUnitBeforeValue("before");
  gpm->setDefaultUnitAfterValue("after");
  gpm->setDefaultValuePrecision(3);  // 0..4 - number of decimal places

  // Set some initial value of measurement
  gpm->setValue(3.1415);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();

  static uint32_t lastTime = 0;
  if (millis() - lastTime > 1000) {
    lastTime = millis();
    // set some new value on gpm:
    gpm->setValue(millis() / 1000.0);
  }
}

