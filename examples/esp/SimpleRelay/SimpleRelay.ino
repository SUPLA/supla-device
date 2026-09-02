// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file SimpleRelay.ino
 * @brief A minimal SUPLA relay example for ESP8266/ESP32 using Arduino.
 * This example shows a single relay channel controlled from the SUPLA app
 * or a local button. It also enables config mode with a web UI for Wi-Fi and
 * SUPLA server parameters.
 *
 * Adjust GPIO numbers for your hardware.
 *
 * @tags simple, relay, button, esp, esp32, esp8266, wifi
 */

#define BUTTON_CFG_GPIO 0
#define STATUS_LED_GPIO 2
#define RELAY_GPIO 12

#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/relay.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/eeprom.h>
#include <supla/storage/littlefs_config.h>

Supla::Eeprom eeprom;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // Channel #0 - Relay
  auto relay = new Supla::Control::Relay(RELAY_GPIO);
  relay->setDefaultFunction(SUPLA_CHANNELFNC_POWERSWITCH);

  // Config + control button
  auto buttonCfg = new Supla::Control::Button(BUTTON_CFG_GPIO, true, true);
  buttonCfg->configureAsConfigButton(&SuplaDevice);
  buttonCfg->addAction(Supla::TOGGLE, relay, Supla::ON_CLICK_1);

  // HTML www components
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

