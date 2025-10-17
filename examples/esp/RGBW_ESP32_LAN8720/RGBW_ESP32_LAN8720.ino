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
/*
  -(50MHz) Osc. Enable to GND -
  GPIO22 - EMAC_TXD1   : TX1
  GPIO19 - EMAC_TXD0   : TX0
  GPIO21 - EMAC_TX_EN  : TX_EN
  GPIO26 - EMAC_RXD1   : RX1
  GPIO25 - EMAC_RXD0   : RX0
  GPIO27 - EMAC_RX_DV  : CRS
  GPIO17 - EMAC_TX_CLK : nINT/REFCLK (50MHz)
  GPIO23 - SMI_MDC     : MDC
  GPIO18 - SMI_MDIO    : MDIO
  GND                  : GND
  3V3                  : VCC
*/

// Other ethernet configurations
// Supla::ESPETH Eth(
//      type, phy_addr, mdc_gpio, mdio_gpio, power_gpio, clk_mode)
//       \/                                                 \/
//  --  type  --                                     ---  clk_mode  ---
//  ETH_PHY_LAN8720                                  ETH_CLOCK_GPIO0_IN
//  ETH_PHY_TLK110                                   ETH_CLOCK_GPIO0_OUT
//  ETH_PHY_RTL8201                                  ETH_CLOCK_GPIO16_OUT
//  ETH_PHY_DP83848                                  ETH_CLOCK_GPIO17_OUT
//  ETH_PHY_KSZ8041                                " EMAC_CLK_EXT_IN " - esp32P4
//  ETH_PHY_KSZ8081

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

#define RED_PIN              4
#define GREEN_PIN            5
#define BLUE_PIN             12
#define BRIGHTNESS_PIN       13
#define BUTTON_PIN           15
#define STATUS_LED_GPIO 2

Supla::ESPETH Eth(1);  // uint_t ETH_ADDR = I²C-address of Ethernet PHY (0 or 1)
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

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
