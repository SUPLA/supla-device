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
 * @file SHT10.ino
 * @author veeroos
 * @brief Example of connecting an SHT10 temperature and humidity sensor to SUPLA using an ESP32.
 * This example configures an ESP device with Wi-Fi to read data from an SHT10 sensor and integrate it with the SUPLA cloud.
 * It includes a web server for Wi-Fi and SUPLA server configuration.
 * It requires the SHT1x sensor library for ESPx by beegee_tokyo to be installed from Arduino library or https://github.com/beegee-tokyo/SHT1x-ESP
 * Users need to adjust network settings. A status LED is also configured.
 *
 * @tags SHT10, temperature, humidity, sensor, esp, esp32, wifi
 */

#define STATUS_LED_GPIO 2
#define BUTTON_CFG_GPIO 0
#define Data_Pin_ 4
#define Clock_Pin_ 5

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/button.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/SHT10.h>
#include <supla/events.h>

// Choose where Supla should store roller shutter data in persistent memory
// We recommend to use external FRAM memory
// #define FRAM_CS_PIN 10
// #define STORAGE_OFFSET 100
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(FRAM_CS_PIN, STORAGE_OFFSET);
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;

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

  auto buttonCfg = new Supla::Control::Button(BUTTON_CFG_GPIO, true, true);

  new Supla::Sensor::SHT10(Data_Pin_, Clock_Pin_);

  buttonCfg->configureAsConfigButton(&SuplaDevice);
  //SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode); //This option will enter configuration mode after ESP startup. Using it is insecure  
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
