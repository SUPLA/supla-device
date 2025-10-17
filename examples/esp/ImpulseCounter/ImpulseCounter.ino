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

#include <SuplaDevice.h>
#include <supla/sensor/impulse_counter.h>
#include <supla/actions.h>
#include <supla/control/internal_pin_output.h>
#include <supla/device/status_led.h>
#include <supla/events.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>

// Choose where Supla should store state data in persistent memory
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // CHANNEL0 - Impulse Counter on pin 34, counting raising edge (from LOW to
  // HIGH), no pullup on pin, and 10 ms debounce timeout
  auto ic1 = new Supla::Sensor::ImpulseCounter(34, true, false, 10);

  // CHANNEL1 - Impulse Counter on pin 35, counting falling edge (from HIGH to
  // LOW), with pullup on pin, and 50 ms debounce timeout
  auto ic2 = new Supla::Sensor::ImpulseCounter(35, false, true, 50);

  // Configuring internal LED to notify each change of impulse counter
  auto led1 = new Supla::Control::InternalPinOutput(24);  // LED on pin 24
  auto led2 = new Supla::Control::InternalPinOutput(25);  // LED on pin 25

  // LED1 will blink (100 ms) on each change of ic1
  led1->setDurationMs(100);
  ic1->addAction(Supla::TURN_ON, led1, Supla::ON_CHANGE);

  // LED2 will toggle it's state on each change of ic2
  ic2->addAction(Supla::TOGGLE, led2, Supla::ON_CHANGE);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
