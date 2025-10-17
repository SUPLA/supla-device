/*
  Copyright (C) malarz

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

/* This example shows ESP82xx/ESP32 based device with 
 * 1) PM1006K semsor from Ikea Vindriktning
 * 2) board LaskaKit ESP-VINDRIKTNING ESP-32 v3.0
 *    https://www.laskakit.cz/en/laskakit-esp-vindriktning-esp-32-i2c/
 * 3) Bosh BME280 sensor
 *
 * It includes
 * 1) simple WebInterface
 * 2) I2C scnanner
 * 3) Update Button
 *
 * A few elements from ESP-VINDRIKTNING are not used/supported in this example:
 * 1) 3 RGB LEDs
 * 2) Ambient light sensor (InfeaRed light sensor)
 * 3) buzzer
 * 4) IR remote controller receiver
 */

#define DEV_NAME "LaskaKit VINDRIKTNING"

#define PM_RX_PIN  16
#define PM_TX_PIN  17
#define FAN_PIN    12

#define SDA_PIN    21
#define SCL_PIN    22

#define LED_PIN    25
#define IR_RC_PIN  26
#define BUZZER_PIN 2
#define AMBIENT_LIGHT_PIN 33

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/version.h>
#include <supla/sensor/BME280.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/network/html/i2cscanner.h>
#include <supla/network/html/button_update.h>
#include <supla/sensor/particle_meter_pm1006k.h>
#include <supla/protocol/aqi.eco.h>
#include <supla/network/html/custom_text_parameter.h>

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // WevInterface with Update
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::ButtonUpdate(&suplaServer);

  // I2C start & scan
  Wire.begin(SDA_PIN, SCL_PIN);
  new Supla::Html::I2Cscanner();

  // Cubic PM1006K
  auto pm1006k = new Supla::Sensor::ParticleMeterPM1006K(PM_RX_PIN, PM_TX_PIN, FAN_PIN, 180, 30);

  // Bosh BME280
  auto bme280 = new Supla::Sensor::BME280(0x77, 100);

  // start parameters from memory
  Supla::Storage::Init();

  // create optional PM10 Channel on pm1006K sensor to read PM10 calculated value (need to send to aqi.eco)
  pm1006k->createPM10Channel();

  // aqi.eco sender
  const char AQIPARAM[] = "aqitk";
  new Supla::Html::CustomTextParameter(AQIPARAM, "aqi.eco Token", 32);
  char token[33] = {};
  Supla::Storage::ConfigInstance()->getString(AQIPARAM, token, 33);
  auto aqieco = new Supla::Protocol::AQIECO(&wifi, token, 120);
  aqieco->addSensor(Supla::SenorType::PM2_5, pm1006k->getPM2_5channel());
  aqieco->addSensor(Supla::SenorType::PM10, pm1006k->getPM10channel());
  aqieco->addSensor(Supla::SenorType::TEMP, bme280->getChannel());
  aqieco->addSensor(Supla::SenorType::HUMI, bme280->getChannel());
  aqieco->addSensor(Supla::SenorType::PRESS, bme280->getSecondaryChannel());

  SuplaDevice.setName(DEV_NAME);
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
