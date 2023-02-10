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
#include <supla/sensor/impulse_counter.h>
// Remove below line if you don't want to have internal LED blinking on each
// impulse
#include <supla/actions.h>
#include <supla/control/internal_pin_output.h>
#include <supla/events.h>

// Choose where Supla should store counter data in persistant memory
// We recommend to use external FRAM memory
#define STORAGE_OFFSET 100
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom(STORAGE_OFFSET);
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

// Choose proper network interface for your card:
#ifdef ARDUINO_ARCH_AVR
// Arduino Mega with EthernetShield W5100:
#include <supla/network/ethernet_shield.h>
// Ethernet MAC address
uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
Supla::EthernetShield ethernet(mac);

// Arduino Mega with ENC28J60:
// #include <supla/network/ENC28J60.h>
// Supla::ENC28J60 ethernet(mac);
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
// ESP8266 and ESP32 based board:
#include <supla/network/esp_wifi.h>
Supla::ESPWifi wifi("your_wifi_ssid", "your_wifi_password");
#endif

void setup() {
  Serial.begin(115200);

  // Replace the falowing GUID with value that you can retrieve from
  // https://www.supla.org/arduino/get-guid
  char GUID[SUPLA_GUID_SIZE] = {0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00,
                                0x00};

  // Replace the following AUTHKEY with value that you can retrieve from:
  // https://www.supla.org/arduino/get-authkey
  char AUTHKEY[SUPLA_AUTHKEY_SIZE] = {0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00,
                                      0x00};

  /*
   * Having your device already registered at cloud.supla.org,
   * you want to change CHANNEL sequence or remove any of them,
   * then you must also remove the device itself from cloud.supla.org.
   * Otherwise you will get "Channel conflict!" error.
   */

  // CHANNEL0 - Impulse Counter on pin 34, counting raising edge (from LOW to
  // HIGH), no pullup on pin, and 10 ms debounce timeout
  auto ic1 = new Supla::Sensor::ImpulseCounter(34, true, false, 10);

  // CHANNEL1 - Impulse Counter on pin 35, counting falling edge (from HIGH to
  // LOW), with pullup on pin, and 50 ms debounce timeout
  auto ic2 = new Supla::Sensor::ImpulseCounter(35, false, true, 50);

  // Configuring internal LED to notify each change of impulse counter
  auto led1 = new Supla::Control::InternalPinOutput(24);  // LED on pin 24
  auto led2 = new Supla::Control::InternalPinOutput(25);  // LED on pin 25

  // LED1 will blink (100 ms) on each change of ic1
  led1->setDurationMs(100);
  ic1->addAction(Supla::TURN_ON, led1, Supla::ON_CHANGE);

  // LED2 will toggle it's state on each change of ic2
  ic2->addAction(Supla::TOGGLE, led2, Supla::ON_CHANGE);

  /*
   * Server address is available at https://cloud.supla.org
   * If you do not have an account, you can create it at
   * https://cloud.supla.org/account/create SUPLA and SUPLA CLOUD are free of
   * charge
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
