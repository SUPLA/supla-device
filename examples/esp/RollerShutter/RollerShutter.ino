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
 * @file RollerShutter.ino
 * @brief Example of controlling a roller shutter directly with an ESP8266/ESP32 for SUPLA integration.
 * This example demonstrates how to control a roller shutter using dedicated GPIO pins on an ESP device
 * for motor control (up/down/stop) and physical buttons for local operation.
 * It also includes status LEDs to indicate the state of the roller shutter.
 * The system integrates with the SUPLA cloud via Wi-Fi and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for motor control, buttons, and status LEDs in the code.
 *
 * @tags roller_shutter, button, relay, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/pin_status_led.h>
#include <supla/control/roller_shutter.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>

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

  Supla::Control::RollerShutter *rs =
      new Supla::Control::RollerShutter(30, 31, false);
  Supla::Control::Button *buttonOpen =
      new Supla::Control::Button(28, true, true);
  Supla::Control::Button *buttonClose =
      new Supla::Control::Button(29, true, true);

  // Add two LEDs to inform about roller shutter relay status
  // If inverted value is required, please add third parameter with true value
  new Supla::Control::PinStatusLed(
      30, 24);  // pin 30 status to be informed on pin 24
  new Supla::Control::PinStatusLed(
      31, 25);  // pin 31 status to be informed on pin 25

  // TODO(anyone): replace with addButton method on rs instance
  buttonOpen->addAction(Supla::OPEN_OR_STOP, *rs, Supla::ON_PRESS);
  buttonClose->addAction(Supla::CLOSE_OR_STOP, *rs, Supla::ON_PRESS);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
