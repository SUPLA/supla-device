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

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/light_relay.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/actions.h>
#include <supla/condition.h>
#include <supla/storage/eeprom.h>

/**
 * @supla-example
 * @file MultiDsBasic.ino
 * @brief DS18B20 multi-sensor manager example (single GPIO / OneWire).
 *
 * This example demonstrates a manager class that handles multiple
 * DS18B20 thermometers connected to a single OneWire bus (one GPIO).
 *
 * Pairing mode can be triggered by the user in SUPLA Cloud.
 * During pairing, the manager searches for sensors on the bus and
 * registers them until the configured device limit is reached or the
 * pairing timeout expires.
 *
 * Each paired thermometer is assigned a channel number (optionally
 * using a channel offset) and becomes available to the application.
 *
 * Users need to adjust the GPIO pin used for OneWire.
 *
 * @dependency https://github.com/PaulStoffregen/OneWire
 * @dependency https://github.com/milesburton/Arduino-Temperature-Control-Library
 * @tags Handler, DS18B20, temperature, sensor, relay, esp, esp32, esp8266, wifi, web_interface, EEPROM
 */

#include <supla/sensor/multi_ds_handler.h>

#define STATUS_LED_GPIO 2
#define HANDLER_GPIO 18
#define FIRST_RELAY_GPIO 19
#define SECOND_RELAY_GPIO 20

Supla::ESPWifi wifi;
Supla::Eeprom eeprom;
Supla::LittleFsConfig configSupla;
Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Channel 0
  new Supla::Control::LightRelay(FIRST_RELAY_GPIO);

  // Handler (channels 1-5)
  auto handler = new Supla::Sensor::MultiDsHandler(&SuplaDevice, HANDLER_GPIO);
  handler->setChannelNumberOffset(1);
  handler->setMaxDeviceCount(5);
  handler->disableSensorsChannelState();

  // Channel 6
  auto relay = new Supla::Control::LightRelay(SECOND_RELAY_GPIO);
  relay->getChannel()->setChannelNumber(6);

  // Actions dynamically attached to first paired thermometer
  handler->addAction(0, Supla::TURN_ON, relay, OnLess(25));
  handler->addAction(0, Supla::TURN_OFF, relay, OnGreater(26));

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setName("MultiDS");
  SuplaDevice.begin(28);
}

void loop() {
  SuplaDevice.iterate();
}
