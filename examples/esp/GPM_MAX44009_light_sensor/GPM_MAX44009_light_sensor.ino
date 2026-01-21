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
 * @file GPM_MAX44009_light_sensor.ino
 * @brief Example of connecting a MAX44009 ambient light sensor to SUPLA using an ESP8266/ESP32.
 * This example demonstrates how to integrate a MAX44009 digital ambient light sensor (connected via I2C)
 * with an ESP device and send its illuminance readings to the SUPLA cloud.
 * It uses Wi-Fi for network connectivity and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust I2C GPIO pins (SDA, SCL), and optionally, the MAX44009 I2C address and units (lux or kilolux) in the code.
 * A status LED is also configured.
 *
 * @dependency https://github.com/RobTillaart/Max44009
 * @tags MAX44009, light, ambient, illuminance, sensor, lux, kilolux, I2C, esp, esp32, esp8266, wifi, web_interface
 */

// Please adjust GPIO to your board configuration
#define STATUS_LED_GPIO 2
#define I2C_SCL 5
#define I2C_SDA 4
#define MAX44009_I2C_ADDR 0x4A  // default, alternative: 0x4B

#include <Wire.h>

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>

#include <supla/sensor/max44009.h>

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

  // Initialize I2C Wire interface
  Wire.begin(I2C_SDA, I2C_SCL);

  // Channels configuration
  // Channel #0 MAX44009 with "lx" units
  auto lightSensor = new Supla::Sensor::Max44009(MAX44009_I2C_ADDR, &Wire);

  lightSensor->setInitialCaption("Light sensor 🌞");

  // If you want to change default units to klx, then uncomment below lines:
  // lightSensor->setDefaultUnitAfterValue("klx");
  // lightSensor->setDefaultValueDivider(1000000);  // in 0.001 units

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}



