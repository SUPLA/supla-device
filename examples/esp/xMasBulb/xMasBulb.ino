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

#define DEV_NAME "Malarz Bombka"
#define DEV_VERSION "2025.12.28"

#define STATUS_LED_GPIO 2
#define RGB_GPIO 14
#define RGB_RING_NUM_LEDS 24

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/version.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/network/html/button_update.h>
#include <supla/control/addressable_leds.h>
#include "addressable_leds_control.h"

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state

// xMas logo in WebInterface is created
// by replacing standard HtmlGenerator
// with a specially prepared version with a changed logo
#include "xMasHtmlGenerator.h"
auto XMasGenerator = new xMasHtmlGenerator;
Supla::EspWebServer suplaServer(XMasGenerator);

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::ButtonUpdate(&suplaServer);

  // WS2812b LEDs strip
  auto strip = new Supla::Control::AddressableLEDs(RGB_RING_NUM_LEDS, RGB_GPIO);

  // Manage single color in App
  auto cs = new AddressableLEDsColorSelector(strip);

  // Effevt switches in App
  auto sw1 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::STILL, 200, 20);
  sw1->getChannel()->setInitialCaption("VEEROOS");
  sw1->getChannel()->setDefaultIcon(1);
  auto sw2 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::SWAP, 200);
  sw2->getChannel()->setInitialCaption("SWAP");
  sw2->getChannel()->setDefaultIcon(1);
  auto sw3 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::FLOW, 200);
  sw3->getChannel()->setInitialCaption("FLOW");
  sw3->getChannel()->setDefaultIcon(1);
  auto sw4 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::RAINBOWWHEEL, 30);
  sw4->getChannel()->setInitialCaption("RAINBOWWHEEL");
  sw4->getChannel()->setDefaultIcon(1);
  auto sw5 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::RAINBOW, 50);
  sw5->getChannel()->setInitialCaption("RAINBOW");
  sw5->getChannel()->setDefaultIcon(1);
  auto sw6 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::RAINBOW, 10);
  sw6->getChannel()->setInitialCaption("RAINBOW FAST");
  sw6->getChannel()->setDefaultIcon(1);
  auto sw7 = new AddressableLEDsEffectSwitch(strip, Supla::Control::AddressableLEDsEffect::RAINBOW, 10, 20);
  sw7->getChannel()->setInitialCaption("RAINBOW VEEROOS");
  sw7->getChannel()->setDefaultIcon(1);
  sw7->setDefaultStateOn();

  SuplaDevice.setName(DEV_NAME);
  SuplaDevice.setSwVersion(DEV_VERSION);
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.setPermanentWebServer();
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
