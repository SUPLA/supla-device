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
#include <supla/control/rgbw_leds.h>
#include <supla/control/button.h>
#include <supla/network/esp32eth.h>
#include <supla/storage/littlefs_config.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>

#define ETH_TYPE ETH_PHY_LAN8720
// Pin# of the I²C clock signal for the Ethernet PHY
#define ETH_MDC_PIN 23
// Pin# of the I²C IO signal for the Ethernet PHY
#define ETH_MDIO_PIN 18
#define ETH_POWER_PIN 16
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN

#define RED_PIN              4
#define GREEN_PIN            5
#define BLUE_PIN             12
#define BRIGHTNESS_PIN       14
#define BUTTON_PIN           15
#define STATUS_LED_GPIO 2

Supla::ESPETH Eth(ETH_PHY_LAN8720,
                  1,  // uint_t ETH_ADDR = I²C-address of Ethernet PHY (0 or 1)
                  ETH_MDC_PIN,
                  ETH_MDIO_PIN,
                  ETH_POWER_PIN,
                  ETH_CLK_MODE);
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // CHANNEL0 - RGB controller and dimmer (RGBW)
  auto rgbw = new Supla::Control::RGBWLeds(
    RED_PIN, GREEN_PIN, BLUE_PIN, BRIGHTNESS_PIN);

  auto button = new Supla::Control::Button(BUTTON_PIN, true, true);
  button->setMulticlickTime(200);
  button->setHoldTime(400);
  button->repeatOnHoldEvery(200);

  button->addAction(Supla::ITERATE_DIM_ALL, rgbw, Supla::ON_HOLD);
  button->addAction(Supla::TOGGLE, rgbw, Supla::ON_CLICK_1);

  Eth.setSSLEnabled(false);  // disable SSL
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}

