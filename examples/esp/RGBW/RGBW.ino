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
 * @file RGBW.ino
 * @brief Example of controlling RGBW LEDs with a button using an ESP8266/ESP32 for SUPLA integration.
 * This example demonstrates how to control the color and brightness of RGBW LEDs (Red, Green, Blue, White)
 * using a momentary button connected to an ESP device. The button can be used to toggle the LEDs
 * and iterate through brightness levels.
 * The device integrates with the SUPLA cloud via Wi-Fi, allowing remote control of the RGBW lighting,
 * and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the RGBW channels and the button in the code.
 * A status LED is also configured.
 *
 * @tags RGBW, LED, dimmer, color, brightness, button, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/control/rgbw_leds.h>
#include <supla/control/button.h>
#include <supla/network/esp_wifi.h>
#include <supla/storage/littlefs_config.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>

#define RED_PIN              4
#define GREEN_PIN            5
#define BLUE_PIN             12
#define BRIGHTNESS_PIN       13
#define BUTTON_PIN           0
#define STATUS_LED_GPIO 2

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

  // CHANNEL0 - RGB controller and dimmer (RGBW)
  auto rgbw = new Supla::Control::RGBWLeds(
      RED_PIN, GREEN_PIN, BLUE_PIN, BRIGHTNESS_PIN);

  auto button = new Supla::Control::Button(BUTTON_PIN, true, true);
  button->setMulticlickTime(200);
  button->setHoldTime(400);
  button->repeatOnHoldEvery(35);

  rgbw->setStep(1);
// rgbw->setMinMaxIterationDelay(750);  // delay between dimming direction
                                        // change, 750 ms (default)
//  rgbw->setMinIterationBrightness(1);  // 1 is default value

  button->addAction(Supla::ITERATE_DIM_ALL, rgbw, Supla::ON_HOLD);
  button->addAction(Supla::TOGGLE, rgbw, Supla::ON_CLICK_1);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
