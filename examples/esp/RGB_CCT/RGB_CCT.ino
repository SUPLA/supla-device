// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file RGB_CCT.ino
 * @brief Example of a five-channel LightingPwmLeds ladder for ESP8266/ESP32.
 *
 * The first channel controls five PWM outputs and supports all available
 * lighting functions, up to RGB+CCT. Each following channel is a child of the
 * previous one and exposes one fewer output, resulting in a progressively
 * smaller function list.
 *
 * Adjust the GPIO numbers to match the hardware. The defaults are intended
 * for ESP8266 ESP-12/NodeMCU and classic ESP32 boards. Other ESP32 variants
 * have different flash, USB and strapping-pin restrictions.
 *
 * @tags RGB, CCT, PWM, LED, parent, esp, esp32, esp8266, wifi, web_interface
 */

#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/lighting_pwm_leds.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>

// Configuration button and status LED. Adjust them for the selected board.
#define BUTTON_CFG_GPIO 0
#define STATUS_LED_GPIO 2

// Five PWM outputs: red, green, blue, warm white and cold white.
#define RED_GPIO 5
#define GREEN_GPIO 4
#define BLUE_GPIO 14
#define WARM_WHITE_GPIO 12
#define COLD_WHITE_GPIO 13

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  auto buttonCfg = new Supla::Control::Button(BUTTON_CFG_GPIO, true, true);
  buttonCfg->configureAsConfigButton(&SuplaDevice);

  // CHANNEL0 - five outputs: DIMMER, RGB, DIMMER+RGB, CCT and RGB+CCT.
  auto rgbCct = new Supla::Control::LightingPwmLeds(
      nullptr,
      RED_GPIO,
      GREEN_GPIO,
      BLUE_GPIO,
      WARM_WHITE_GPIO,
      COLD_WHITE_GPIO);

  // Each child shares the physical outputs with its parent, but starts one
  // output later. Its available function list is reduced automatically.
  // CHANNEL1 - four outputs: DIMMER, RGB, DIMMER+RGB and CCT.
  auto dimmerRgb = new Supla::Control::LightingPwmLeds(
      rgbCct, GREEN_GPIO, BLUE_GPIO, WARM_WHITE_GPIO, COLD_WHITE_GPIO, -1);

  // CHANNEL2 - three outputs: DIMMER, RGB and CCT.
  auto rgb = new Supla::Control::LightingPwmLeds(
      dimmerRgb, BLUE_GPIO, WARM_WHITE_GPIO, COLD_WHITE_GPIO, -1, -1);

  // CHANNEL3 - two outputs: DIMMER and CCT.
  auto cct = new Supla::Control::LightingPwmLeds(
      rgb, WARM_WHITE_GPIO, COLD_WHITE_GPIO, -1, -1, -1);

  // CHANNEL4 - one output: DIMMER.
  new Supla::Control::LightingPwmLeds(
      cct, COLD_WHITE_GPIO, -1, -1, -1, -1);

  // HTML www component.
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
