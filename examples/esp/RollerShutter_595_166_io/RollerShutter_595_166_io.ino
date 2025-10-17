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

#define NUMBER_OF_SR595 \
  4  // number of shift registers eg 4 x 74HC595 = 32 outputs  pin 0 - 31
#define DATA_SR595_GPIO  14  // data pin 14 on 74HC595
#define CLOCK_SR595_GPIO 13  // clock pin 11 on 74HC595
#define LATCH_SR595_GPIO 12  // latch pin 12 on 74HC595

#define NUMBER_OF_SR166 \
  4  // number of shift registers eg 4 x 74HC166 = 32 intputs  pin 0 - 31
#define DATA_SR166_GPIO  4  // data pin 13 on 74HC166
#define CLOCK_SR166_GPIO 5  // clock pin 2 on 74HC166
#define LOAD_SR166_GPIO  0  // load pin 1 on 74HC166

#include <SuplaDevice.h>
#include <supla/control/EXT_SR166.h>
#include <supla/control/EXT_SR595.h>
#include <supla/control/button.h>
#include <supla/control/pin_status_led.h>
#include <supla/control/relay.h>
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

  auto ExtSr595 = new Supla::Control::ExtSR595(
      NUMBER_OF_SR595, DATA_SR595_GPIO, CLOCK_SR595_GPIO, LATCH_SR595_GPIO);
  auto ExtSr166 = new Supla::Control::ExtSR166(
      NUMBER_OF_SR166, DATA_SR166_GPIO, CLOCK_SR166_GPIO, LOAD_SR166_GPIO);

  auto r1 = new Supla::Control::Relay(ExtSr595, 0, true);
  auto r2 = new Supla::Control::Relay(ExtSr595, 14, false);
  auto r3 = new Supla::Control::Relay(ExtSr595, 27, true);

  Supla::Control::RollerShutter *rs = new Supla::Control::RollerShutter(
      ExtSr595, 3, 4, true);  // Q3 and Q4 from first 74HC595

  Supla::Control::Button *buttonOpen = new Supla::Control::Button(ExtSr166, 4);
  Supla::Control::Button *buttonClose = new Supla::Control::Button(ExtSr166, 5);

  // Add two LEDs to inform about roller shutter relay status
  // If inverted value is required, please add parameter with true value
  new Supla::Control::PinStatusLed(
      NULL,
      ExtSr595,
      15,
      30,
      true);  // gpio 15 on ESP status to be informed on pin 30 of 74HC595
  new Supla::Control::PinStatusLed(
      ExtSr595,
      ExtSr595,
      4,
      31,
      true);  // pin 4 on 74HC595 status to be informed on pin 31 of 74HC595

  buttonOpen->addAction(Supla::OPEN_OR_STOP, *rs, Supla::ON_PRESS);
  buttonClose->addAction(Supla::CLOSE_OR_STOP, *rs, Supla::ON_PRESS);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
