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
 * @file WebInterface.ino
 * @brief Comprehensive example of a device with a web interface for Wi-Fi and SUPLA configuration, featuring roller shutter and relay control on ESP8266/ESP32.
 * This example demonstrates a device that starts in configuration mode with its own Wi-Fi AP, providing a web interface for:
 * - Configuring Wi-Fi parameters.
 * - Setting up SUPLA server connection details.
 * It includes control for a roller shutter (up/down/stop with dedicated buttons), a general purpose relay,
 * and three buttons (two for roller shutter, one for relay and config mode entry).
 * The device integrates with the SUPLA cloud via Wi-Fi and stores roller shutter data persistently in EEPROM (or optionally FRAM).
 * Users need to adjust network settings and GPIO pins for the roller shutter relays, general relay, and buttons.
 * A status LED is also configured.
 *
 * @tags web_interface, config_mode, roller_shutter, relay, button, action_trigger, wifi, esp, esp32, esp8266, EEPROM, FRAM
 */

#define STATUS_LED_GPIO 2
#define ROLLER_SHUTTER_UP_RELAY_GPIO 4
#define ROLLER_SHUTTER_DOWN_RELAY_GPIO 5
#define RELAY_GPIO 12
#define BUTTON_UP_GPIO 13
#define BUTTON_DOWN_GPIO 14
#define BUTTON_CFG_RELAY_GPIO 0

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/relay.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>

// Choose where Supla should store roller shutter data in persistent memory
// We recommend to use external FRAM memory
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

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

  // Channels configuration
  // CH 0 - Roller shutter
  auto rs = new Supla::Control::RollerShutter(
      ROLLER_SHUTTER_UP_RELAY_GPIO, ROLLER_SHUTTER_DOWN_RELAY_GPIO, true);
  // CH 1 - Relay
  auto r1 = new Supla::Control::Relay(RELAY_GPIO);
  // CH 2 - Action trigger
  auto at1 = new Supla::Control::ActionTrigger();
  // CH 3 - Action trigger
  auto at2 = new Supla::Control::ActionTrigger();
  // CH 4 - Action trigger
  auto at3 = new Supla::Control::ActionTrigger();

  // Buttons configuration
  auto buttonOpen = new Supla::Control::Button(BUTTON_UP_GPIO, true, true);
  auto buttonClose = new Supla::Control::Button(BUTTON_DOWN_GPIO, true, true);
  auto buttonCfgRelay =
    new Supla::Control::Button(BUTTON_CFG_RELAY_GPIO, true, true);

  buttonOpen->addAction(Supla::OPEN_OR_STOP, *rs, Supla::ON_PRESS);
  buttonOpen->setHoldTime(1000);
  buttonOpen->setMulticlickTime(300);

  buttonClose->addAction(Supla::CLOSE_OR_STOP, *rs, Supla::ON_PRESS);
  buttonClose->setHoldTime(1000);
  buttonClose->setMulticlickTime(300);

  buttonCfgRelay->configureAsConfigButton(&SuplaDevice);
  buttonCfgRelay->addAction(Supla::TOGGLE, r1, Supla::ON_CLICK_1);

  // Action trigger configuration
  at1->setRelatedChannel(rs);
  at1->attach(buttonOpen);

  at2->setRelatedChannel(rs);
  at2->attach(buttonClose);

  at3->setRelatedChannel(r1);
  at3->attach(buttonCfgRelay);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
