/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifdef ARDUINO_ARCH_AVR
#error "This example is not supported on Arduino AVR"
#endif

#include <SuplaDevice.h>
#include <supla/storage/eeprom.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/control/hvac_base.h>
#include <supla/clock/clock.h>
#include <supla/network/html/time_parameters.h>
#include <supla/network/html/hvac_parameters.h>
#include <supla/control/internal_pin_output.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/sensor/virtual_thermometer.h>
#include <supla/control/button.h>

/*
  This example requires Dallas Temperature Control library installed.
  https://github.com/milesburton/Arduino-Temperature-Control-Library
*/
// Add include to DS sensor
#include <supla/sensor/DS18B20.h>

#define CFG_BUTTON_GPIO 0
#define STATUS_LED_GPIO 2
#define LED_RED_GPIO 13
#define LED_BLUE_GPIO 12
#define OUTPUT_GPIO 14
#define DS18B20_GPIO 4


Supla::ESPWifi wifi;
Supla::Eeprom eeprom;
Supla::LittleFsConfig configSupla;
Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // WARNING: using default Clock class. It works only when there is connection
  // with Supla server. In real thermostat application, use RTC HW clock,
  // which will keep time after ESP power reset
  new Supla::Clock;

  // HTML www component (they appear in sections according to creation
  // sequence)
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;
  new Supla::Html::TimeParameters(&SuplaDevice);

  new Supla::Device::StatusLed(STATUS_LED_GPIO, true);
  eeprom.setStateSavePeriod(5000);

  auto output = new Supla::Control::InternalPinOutput(OUTPUT_GPIO);
  auto redStatusLed = new Supla::Control::InternalPinOutput(LED_RED_GPIO,
      false);
  auto blueStatusLed = new Supla::Control::InternalPinOutput(LED_BLUE_GPIO,
      false);

  // Channel 0
  auto hvac = new Supla::Control::HvacBase(output);
  // Html page for HVAC
  new Supla::Html::HvacParameters(hvac);

  // Channel 1-2 - Thermometer DS18B20
  // 2 DS18B20 thermometers. DS address can be omitted when there is
  // only one device at a pin
  DeviceAddress ds1addr = {0x28, 0xFF, 0xC8, 0xAB, 0x6E, 0x18, 0x01, 0xFC};
  DeviceAddress ds2addr = {0x28, 0xFF, 0x54, 0x73, 0x6E, 0x18, 0x01, 0x77};

  new Supla::Sensor::DS18B20(DS18B20_GPIO, ds1addr);
  new Supla::Sensor::DS18B20(DS18B20_GPIO, ds2addr);

  // If you don't have thermometers, but still want to try how thermostat works,
  // comment above lines and uncomment below lines, to configure dummy
  // thermometers (they wil report temperature which is configured below)

  //  auto t1 = new Supla::Sensor::VirtualThermometer;
  //  auto t2 = new Supla::Sensor::VirtualThermometer;
  //  t1->setValue(23.1);
  //  t2->setValue(26.2);

  // configure red and blue LED behavior
  hvac->addAction(Supla::TURN_OFF, redStatusLed, Supla::ON_HVAC_STANDBY, true);
  hvac->addAction(Supla::TURN_OFF, blueStatusLed, Supla::ON_HVAC_STANDBY, true);
  hvac->addAction(Supla::TURN_OFF, redStatusLed, Supla::ON_HVAC_COOLING, true);
  hvac->addAction(Supla::TURN_OFF, blueStatusLed, Supla::ON_HVAC_HEATING, true);
  hvac->addAction(Supla::TURN_ON, redStatusLed, Supla::ON_HVAC_HEATING, true);
  hvac->addAction(Supla::TURN_ON, blueStatusLed, Supla::ON_HVAC_COOLING, true);

  // Configure thermostat parameters
  hvac->setMainThermometerChannelNo(1);     // Main Thermometer DS1
  hvac->setAuxThermometerChannelNo(2);      // Aux Thermometer DS2
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR);
  hvac->setTemperatureHisteresisMin(20);  // 0.2 degree
  hvac->setTemperatureHisteresisMax(1000);  // 10 degree
  hvac->setTemperatureAuxMin(500);  // 5 degrees
  hvac->setTemperatureAuxMax(7500);  // 75 degrees

  hvac->setTemperatureHisteresis(40);

// To change default function to "cooling", uncomment below line. This setting
// will be applied only on first startup. On following startups, it is restored
// from flash memory
// hvac->getChannel()->setDefault(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_COOL);

  Supla::Channel::reg_dev.Flags |= SUPLA_DEVICE_FLAG_DEVICE_CONFIG_SUPPORTED;

  auto cfgButton = new Supla::Control::Button(CFG_BUTTON_GPIO, true, true);
  cfgButton->configureAsConfigButton(&SuplaDevice);

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  SuplaDevice.setName("Basic thermostat");
  SuplaDevice.begin(21); // Thermostat requires proto version >= 21
}

void loop() {
  SuplaDevice.iterate();
}


