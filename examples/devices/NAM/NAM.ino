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

#define NAM_VERSION "0.5"
#define DEV_NAME "NAM 0.3.3"

#ifdef ARDUINO_ARCH_ESP32

// PINs of Wemos D1 mini ESP32
#define STATUS_LED_GPIO 2
#define SDS_RX_PIN 22  // D1
#define SDS_TX_PIN 21  // D2
#define SDA_PIN 17     // D3
#define SCL_PIN 16     // D4
#define ONEWIRE_PIN 23 // D7

#else

// PINs of Wemos D1 mini 8266
#define STATUS_LED_GPIO 16 // not connected
#define SDS_RX_PIN 5   // D1
#define SDS_TX_PIN 4   // D2
#define SDA_PIN 0      // D3
#define SCL_PIN 2      // D4
#define ONEWIRE_PIN 13 // D7

#endif

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/storage.h>
#include <supla/storage/littlefs_config.h>
#include <supla/version.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/network/html/button_update.h>
#include <supla/network/html/i2cscanner.h>

#include <supla/sensor/BME280.h>
#include <supla/sensor/DS18B20.h>
#include <supla/sensor/bh1750.h>
#include <supla/sensor/particle_meter_sds011.h>
#include <supla/sensor/sht3x.h>
#include "heca.h"

#ifdef ARDUINO_ARCH_ESP32
#include <supla/protocol/aqi.eco.h>
#include <supla/network/html/custom_text_parameter.h>
#endif

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO); // inverted state

Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;
  new Supla::Html::ButtonUpdate(&suplaServer);

  // start parameters from memory
  Supla::Storage::Init();

  // I2C start
  Wire.begin(SDA_PIN, SCL_PIN);
  new Supla::Html::I2Cscanner();

  // Bosh BME280
  auto bme280 = new Supla::Sensor::BME280(0x76, 100);

  // Dallas Semiconductor DS18b20
  auto ds18b20 = new Supla::Sensor::DS18B20(ONEWIRE_PIN);

  // Rohm Semiconductor BH1750FVI
  auto bh1750 = new Supla::Sensor::Bh1750(0x23);
  bh1750->setInitialCaption("Light sensor");

  // NovaFitness SDS011
  auto sds011 = new Supla::Sensor::ParticleMeterSDS011(SDS_RX_PIN, SDS_TX_PIN, 1);

  // Netigo HECA kit
  auto heca = new Supla::Sensor::HECA();

#ifdef ARDUINO_ARCH_ESP32
  // aqi.eco sender
  const char AQIPARAM[] = "aqitk";
  new Supla::Html::CustomTextParameter(AQIPARAM, "aqi.eco Token", 32);
  char token[33] = {};
  Supla::Storage::ConfigInstance()->getString(AQIPARAM, token, 33);
  auto aqieco = new Supla::Protocol::AQIECO(&wifi, token, 120);
  aqieco->addSensor(Supla::SenorType::PM2_5, sds011->getPM2_5channel());
  aqieco->addSensor(Supla::SenorType::PM10, sds011->getPM10channel());
  aqieco->addSensor(Supla::SenorType::TEMP, bme280->getChannel());
  aqieco->addSensor(Supla::SenorType::HUMI, bme280->getChannel());
  aqieco->addSensor(Supla::SenorType::PRESS, bme280->getSecondaryChannel());
  aqieco->addSensor(Supla::SenorType::LIGHT, bh1750->getChannel());
#endif

  SuplaDevice.setName(DEV_NAME);
  const char DeviceVersion[] = "NAM " NAM_VERSION " / " SUPLA_SHORT_VERSION;
  SuplaDevice.setSwVersion(DeviceVersion);
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setPermanentWebServer();
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
