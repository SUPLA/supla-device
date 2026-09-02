// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file RelayRollerShutterPair.ino
 * @brief Example of using two RelayRollerShutterPair instances with SUPLA on ESP8266/ESP32.
 *
 * The first pair starts as two independent relay channels. The second pair
 * starts as one roller shutter controlled by two relays. Both pairs can be
 * switched between relay and roller shutter functions from the SUPLA
 * configuration.
 *
 * Each pair has two local control buttons. A separate button enters the local
 * configuration mode. Adjust all GPIO numbers to match the selected board and
 * relay/button hardware before uploading the sketch.
 *
 * @tags relay, roller_shutter, relay_roller_shutter_pair, button, cfg_button, wifi, esp, esp32, esp8266, littlefs
 */

#if defined(ESP8266)

// ESP8266 uses almost every GPIO available on common ESP-12 modules.
// GPIO0, GPIO2 and GPIO15 are boot-strapping pins. Keep their required boot
// levels in mind when designing the board. GPIO15 is used as an active-high
// relay output, so its required LOW boot level also keeps that relay off.
#define STATUS_LED_GPIO 2
#define CFG_BUTTON_GPIO 0
#define RELAY_PAIR_OUTPUT_0_GPIO 12
#define RELAY_PAIR_OUTPUT_1_GPIO 13
#define ROLLER_PAIR_OUTPUT_UP_GPIO 14
#define ROLLER_PAIR_OUTPUT_DOWN_GPIO 15
#define RELAY_PAIR_BUTTON_0_GPIO 4
#define RELAY_PAIR_BUTTON_1_GPIO 5

// GPIO16 does not provide the standard INPUT_PULLUP on ESP8266. Connect an
// external pull-up resistor from this pin to 3.3 V.
#define ROLLER_PAIR_BUTTON_UP_GPIO 16
#define ROLLER_PAIR_BUTTON_UP_PULLUP false

// GPIO3 is UART RX. Using it as a button input means that serial reception is
// unavailable after the button is initialized. Serial logging still uses TX.
#define ROLLER_PAIR_BUTTON_DOWN_GPIO 3

#elif defined(ESP32)

// Example ESP32 mapping. All control pins below are regular GPIOs on common
// ESP32-WROOM boards. Adjust them when using a different ESP32 variant.
#define STATUS_LED_GPIO 2
#define CFG_BUTTON_GPIO 0
#define RELAY_PAIR_OUTPUT_0_GPIO 16
#define RELAY_PAIR_OUTPUT_1_GPIO 17
#define ROLLER_PAIR_OUTPUT_UP_GPIO 18
#define ROLLER_PAIR_OUTPUT_DOWN_GPIO 19
#define RELAY_PAIR_BUTTON_0_GPIO 21
#define RELAY_PAIR_BUTTON_1_GPIO 22
#define ROLLER_PAIR_BUTTON_UP_GPIO 25
#define ROLLER_PAIR_BUTTON_UP_PULLUP true
#define ROLLER_PAIR_BUTTON_DOWN_GPIO 26

#else
#error "This example supports only ESP8266 and ESP32 Arduino cores"
#endif

#include <SuplaDevice.h>
#include <supla/control/button.h>
#include <supla/control/relay_roller_shutter_pair.h>
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

  // Channels 0 and 1: two independent relays.
  auto relayPair = new Supla::Control::RelayRollerShutterPair(
      RELAY_PAIR_OUTPUT_0_GPIO, RELAY_PAIR_OUTPUT_1_GPIO, true, false);
  relayPair->setDefaultFunctions(SUPLA_CHANNELFNC_LIGHTSWITCH,
                                 SUPLA_CHANNELFNC_LIGHTSWITCH);
  relayPair->setDefaultStateRestore();

  // Channels 2 and 3: one roller shutter by default. The secondary channel is
  // available as a relay after switching the primary function to a relay
  // function in the channel configuration.
  auto rollerPair = new Supla::Control::RelayRollerShutterPair(
      ROLLER_PAIR_OUTPUT_UP_GPIO, ROLLER_PAIR_OUTPUT_DOWN_GPIO, true, false);
  rollerPair->setDefaultFunctions(
      SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
      SUPLA_CHANNELFNC_LIGHTSWITCH);
  rollerPair->setDefaultStateRestore();

  // Local controls for the relay pair.
  auto relayButton0 =
      new Supla::Control::Button(RELAY_PAIR_BUTTON_0_GPIO, true, true);
  auto relayButton1 =
      new Supla::Control::Button(RELAY_PAIR_BUTTON_1_GPIO, true, true);
  relayPair->attach(relayButton0, relayButton1);

  // Local up/down controls for the roller shutter pair.
  auto rollerButtonUp =
      new Supla::Control::Button(ROLLER_PAIR_BUTTON_UP_GPIO,
                                 ROLLER_PAIR_BUTTON_UP_PULLUP,
                                 true);
  auto rollerButtonDown =
      new Supla::Control::Button(ROLLER_PAIR_BUTTON_DOWN_GPIO, true, true);
  rollerPair->attach(rollerButtonUp, rollerButtonDown);

  // Separate configuration button. Hold it to enter local configuration mode.
  auto cfgButton = new Supla::Control::Button(CFG_BUTTON_GPIO, true, true);
  cfgButton->configureAsConfigButton(&SuplaDevice);

  // HTML web interface components.
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Do not force configuration mode at every boot. The CFG button can be
  // used when local configuration is needed.
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInNotConfiguredMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
