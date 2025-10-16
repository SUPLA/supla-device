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

/* This example shows ESP82xx/ESP32 based device with simple WebInterface
   used to configure Wi-Fi parameters and Supla server connection.
   There is one RollerShutter, one Relay and 3 buttons configured.
   Two buttons are for roller shutter with Action Trigger.
   Third button is for controlling the relay and for switching module to
   config mode.
   After fresh installation, device will be in config mode. It will have its
   own Wi-Fi AP configured. You should connect to it with your mobile phone
   and open http://192.168.4.1 where you can configure the device.
   Status LED is also configured. Please adjust GPIOs to your HW.
*/

/*
   Channels configuration using shift registers:

   - BUTTON_CFG_GPIO: ESP82xx/ESP32 config button to enter setup mode.

   - SR74HC595: 4 x 74HC595 controlling relay outputs.
     r1, r2, r3 are relays connected to channels 0, 14, 27.
     rs is a roller shutter using channels 3 (up) and 4 (down).

   - SR74HC166: 4 x 74HC166 reading button inputs.
     buttonOpen (pin 4) and buttonClose (pin 5) control the roller shutter.

   - PinStatusLed: LEDs showing relay/ESP82xx/ESP32 status.
     - First LED: ESP82xx/ESP32 status (pin 30 on 74HC595)
     - Second LED: Q4 relay status (pin 31 on 74HC595)
     Both LEDs inverted for correct indication.

   - Actions:
     buttonOpen -> rs OPEN_OR_STOP on press
     buttonClose -> rs CLOSE_OR_STOP on press

   This setup allows expanding inputs/outputs via shift registers
   while keeping minimal ESP82xx/ESP32 GPIO usage and full Supla integration.
*/

#include <SuplaDevice.h>
#include <supla/control/action_trigger.h>
#include <supla/control/button.h>
#include <supla/control/pin_status_led.h>
#include <supla/control/relay.h>
#include <supla/control/roller_shutter.h>
#include <supla/device/status_led.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/events.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>
#include <supla/io/SR74HC166.h>
#include <supla/io/SR74HC595.h>

#define STATUS_LED_GPIO 2
#define BUTTON_CFG_GPIO 0

#define NUMBER_OF_SR595 \
  4  // number of shift registers eg 4 x 74HC595 = 32 outputs  pin 0 - 31
#define DATA_SR595_GPIO  13  // data pin 14 on 74HC595
#define CLOCK_SR595_GPIO 12  // clock pin 11 on 74HC595
#define LATCH_SR595_GPIO 14  // latch pin 12 on 74HC595

#define NUMBER_OF_SR166 \
  4  // number of shift registers eg 4 x 74HC166 = 32 intputs  pin 0 - 31
#define DATA_SR166_GPIO  4   // data pin 13 on 74HC166
#define CLOCK_SR166_GPIO 5   // clock pin 2 on 74HC166
#define LOAD_SR166_GPIO  15  // load pin 1 on 74HC166

// Choose where Supla should store roller shutter data in persistent memory
// We recommend to use external FRAM memory
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

#include <supla/network/html/button_update.h>
Supla::Html::ButtonUpdate htmlButtonUpdate(&suplaServer);
#include <supla/network/html/button_refresh.h>
Supla::Html::ButtonRefresh htmlButtonRefresh;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;
  /*
      Having your device already registered at cloud.supla.org,
      you want to change CHANNEL sequence or remove any of them,
      then you must also remove the device itself from cloud.supla.org.
      Otherwise you will get "Channel conflict!" error.
  */

  // Channels configuration
  auto buttonCfg = new Supla::Control::Button(BUTTON_CFG_GPIO, true, true);
  buttonCfg->configureAsConfigButton(&SuplaDevice);

  auto sr74hc595 = new Supla::Io::SR74HC595(
      NUMBER_OF_SR595, DATA_SR595_GPIO, CLOCK_SR595_GPIO, LATCH_SR595_GPIO);
  auto sr74hc166 = new Supla::Io::SR74HC166(
      NUMBER_OF_SR166, DATA_SR166_GPIO, CLOCK_SR166_GPIO, LOAD_SR166_GPIO);

  auto r1 = new Supla::Control::Relay(sr74hc595, 0, true);
  auto r2 = new Supla::Control::Relay(sr74hc595, 14, false);
  auto r3 = new Supla::Control::Relay(sr74hc595, 27, true);

  Supla::Control::RollerShutter *rs = new Supla::Control::RollerShutter(
      sr74hc595, 3, 4, true);  // Q3 and Q4 from first 74HC595

  Supla::Control::Button *buttonOpen = new Supla::Control::Button(sr74hc166, 4);
  Supla::Control::Button *buttonClose =
      new Supla::Control::Button(sr74hc166, 5);

  // Add two LEDs to inform about roller shutter relay status
  // If inverted value is required, please add parameter with true value
  new Supla::Control::PinStatusLed(
      NULL,
      sr74hc595,
      15,
      30,
      true);  // gpio 15 on ESP status to be informed on pin 30 of 74HC595
  new Supla::Control::PinStatusLed(
      sr74hc595,
      sr74hc595,
      4,
      31,
      true);  // pin 4 on 74HC595 status to be informed on pin 31 of 74HC595

  buttonOpen->addAction(Supla::OPEN_OR_STOP, *rs, Supla::ON_PRESS);
  buttonClose->addAction(Supla::CLOSE_OR_STOP, *rs, Supla::ON_PRESS);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartWithCfgModeThenOffline);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}