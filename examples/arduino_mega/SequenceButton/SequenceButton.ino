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
 * @brief Example of a sequence button (rhythm-based) for triggering actions with an Arduino Mega for SUPLA integration.
 * This example demonstrates how to implement a "secret" button sequence or rhythm detection using a `SequenceButton` component.
 * Based on a predefined sequence of button presses and releases, it can trigger different actions,
 * such as activating a "secret" relay or an "alarm" relay if the sequence does not match.
 * The system integrates with the SUPLA cloud via an Ethernet shield (W5100 or ENC28J60).
 * Users need to adjust network settings, SUPLA GUID, AUTHKEY, GPIO pins for the button and relays, and define their desired button sequence in the code.
 *
 * @tags sequence_button, button, rhythm, secret, relay, arduino_mega, ethernet, w5100, enc28j60
 */
#include <SuplaDevice.h>
#include <supla/actions.h>
#include <supla/control/relay.h>
#include <supla/control/sequence_button.h>

// Choose proper network interface for your card:
// Arduino Mega with EthernetShield W5100:
#include <supla/network/ethernet_shield.h>
// Ethernet MAC address
uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
Supla::EthernetShield ethernet(mac);

// Arduino Mega with ENC28J60:
// #include <supla/network/ENC28J60.h>
// Supla::ENC28J60 ethernet(mac);

void setup() {
  Serial.begin(115200);

  // Replace the falowing GUID with value that you can retrieve from
  // https://www.supla.org/arduino/get-guid
  char GUID[SUPLA_GUID_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Replace the following AUTHKEY with value that you can retrieve from:
  // https://www.supla.org/arduino/get-authkey
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00};

  /*
   * Having your device already registered at cloud.supla.org,
   * you want to change CHANNEL sequence or remove any of them,
   * then you must also remove the device itself from cloud.supla.org.
   * Otherwise you will get "Channel conflict!" error.
   */

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

  /*
   * SuplaDevice Initialization.
   * Server address is available at https://cloud.supla.org
   * If you do not have an account, you can create it at
   * https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
   * charge
   *
   */

  SuplaDevice.begin(
      GUID,              // Global Unique Identifier
      "svr1.supla.org",  // SUPLA server address
      "email@address",   // Email address used to login to Supla Cloud
      AUTHKEY);          // Authorization key
}

void loop() {
  SuplaDevice.iterate();
}
