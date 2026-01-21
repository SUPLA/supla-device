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

/**
 * @supla-example
 * @file LaskaKit_VINDRIKTNING.ino
 * @brief Example for the LaskaKit ESP-VINDRIKTNING ESP-32 v3.0 board, integrating particle, temperature, humidity, and pressure sensors with SUPLA.
 * This comprehensive example demonstrates the use of the LaskaKit ESP-VINDRIKTNING board, featuring a PM1006K particle sensor (from IKEA Vindriktning)
 * and a BME280 environmental sensor (temperature, humidity, pressure). It also includes optional integration for SCD41 and SGP41 sensors.
 * The device provides a web interface for configuration, an I2C scanner, an update button, and sends collected sensor data to aqi.eco.
 * It integrates with the SUPLA cloud via Wi-Fi.
 * Network settings are configured via the web interface. Users need to adjust the aqi.eco token in the code.
 *
 * @dependency https://www.laskakit.cz/en/laskakit-esp-vindriktning-esp-32-i2c/
 * @tags LaskaKit, VINDRIKTNING, ESP32, PM1006K, BME280, SCD41, SGP41, particle, temperature, humidity, pressure, air_quality, aqi.eco, web_interface, I2C, wifi
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
#include <supla/storage/storage.h>
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
#include <supla/sensor/SCD4x.h>
#include <supla/sensor/SGP41.h>

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

  //Sensirion SCD41
  auto scd41 = new Supla::Sensor::SCD4x;

  //Sensirion SGP41
  auto sgp41 = new Supla::Sensor::SGP41(bme280);

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
  aqieco->addSensor(Supla::SenorType::CO2, scd41->getCO2channel());
// not implemented in aqi.eco
//  aqieco->addSensor(Supla::SenorType::VOC, sgp41->getVOCchannel());
//  aqieco->addSensor(Supla::SenorType::NOX, sgp41->getNOxchannel());

  SuplaDevice.setSwVersion(SUPLA_SHORT_VERSION);
  SuplaDevice.setName(DEV_NAME);
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setPermanentWebInterface();
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
