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
 * This example requires HX711_ADC library installed.
 * https://github.com/olkal/HX711_ADC
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
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

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
