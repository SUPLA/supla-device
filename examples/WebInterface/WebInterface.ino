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
 * used to configure Wi-Fi parameters and Supla server connection.
 * There is one RollerShutter, one Relay and 3 buttons configured.
 * Two buttons are for roller shutter with Action Trigger.
 * Third button is for controlling the relay and for switching module to
 * config mode.
 * After fresh installation, device will be in config mode. It will have its
 * own Wi-Fi AP configured. You should connect to it with your mobile phone
 * and open http://192.168.4.1 where you can configure the device.
 * Status LED is also configured. Please adjust GPIOs to your HW.
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
#include <supla/device/supla_ca_cert.h>
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


// HTML www component (they appear in sections according to creation
// sequence)
Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;

void setup() {

  Serial.begin(115200);

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

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
