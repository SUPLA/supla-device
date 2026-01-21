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
 * @file SequenceButton.ino
 * @brief Example of a sequence button (rhythm-based) for triggering actions with an ESP8266/ESP32 for SUPLA integration.
 * This example demonstrates how to implement a "secret" button sequence or rhythm detection using a `SequenceButton` component.
 * Based on a predefined sequence of button presses and releases, it can trigger different actions,
 * such as activating a "secret" relay or an "alarm" relay if the sequence does not match.
 * The system integrates with the SUPLA cloud via Wi-Fi and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust GPIO pins for the button and relays, and define their desired button sequence in the code.
 * A status LED is also configured.
 *
 * @tags sequence_button, button, rhythm, secret, relay, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/actions.h>
#include <supla/control/relay.h>
#include <supla/control/sequence_button.h>
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

  auto secretRelay = new Supla::Control::Relay(
      30, false);  // Low level trigger relay on pin 30
  auto alarmRelay = new Supla::Control::Relay(
      31, false);  // Low level trigger relay on pin 31
  auto seqButton = new Supla::Control::SequenceButton(
      28, true, true);  // Button on pin 28 with internal pullUp
                        // and LOW is considered as "pressed" state

  // Sequence of lenghts [ms] of button being presset, released, pressed,
  // released, etc. Aplication will print on Serial recorded sequence, so use it
  // to record your rhythm and put it here
  uint16_t sequence[30] = {90, 590, 90, 140, 90, 290, 90, 230, 90, 140, 90};

  seqButton->setSequence(sequence);
  seqButton->setMargin(0.5);  // we allow +- 50% deviation of state length
                              // compared to matching sequence

  // Button will trigger secretRelay when correct rhythm will be detected or
  // alarmRelay otherwise
  seqButton->addAction(Supla::TURN_ON, secretRelay, Supla::ON_SEQUENCE_MATCH);
  seqButton->addAction(
      Supla::TURN_ON, alarmRelay, Supla::ON_SEQUENCE_DOESNT_MATCH);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
