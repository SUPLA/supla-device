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

// Dependency: Please install "BH1750" by Christopher Laws library in Arduino:
// https://github.com/claws/BH1750

// Please adjust GPIO to your board configuration
#define STATUS_LED_GPIO 2
#define I2C_SCL 5
#define I2C_SDA 4
#define BH1750_I2C_ADDR 0x23  // default, alternative: 0x5C

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
#include <supla/device/supla_ca_cert.h>

#include <supla/sensor/bh1750.h>

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
  // Channel #0 BH1750 with "lx" units
  auto lightSensor = new Supla::Sensor::Bh1750(BH1750_I2C_ADDR);

  // If you want to change default units to klx, then uncomment below lines:
  // lightSensor->setDefaultUnitAfterValue("klx");
  // lightSensor->setDefaultValueDivider(1000000);  // in 0.001 units


  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}

