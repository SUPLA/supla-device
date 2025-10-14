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
#include <supla/device/notifications.h>
#include <supla/time.h>

/*
 * This example requires Dallas Temperature Control library installed.
 * https://github.com/milesburton/Arduino-Temperature-Control-Library
 */
// Add include to DS sensor
#include <supla/sensor/DS18B20.h>

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

Supla::Sensor::DS18B20 *t1 = nullptr;
Supla::Sensor::DS18B20 *t2 = nullptr;
Supla::Sensor::DS18B20 *t3 = nullptr;
Supla::Sensor::DS18B20 *t4 = nullptr;

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

  // CHANNEL0-3 - Thermometer DS18B20
  // 4 DS18B20 thermometers at pin 23. DS address can be omitted when there is
  // only one device at a pin
  DeviceAddress ds1addr = {0x28, 0xFF, 0xC8, 0xAB, 0x6E, 0x18, 0x01, 0xFC};
  DeviceAddress ds2addr = {0x28, 0xFF, 0x54, 0x73, 0x6E, 0x18, 0x01, 0x77};
  DeviceAddress ds3addr = {0x28, 0xFF, 0x55, 0xCA, 0x6B, 0x18, 0x01, 0x8D};
  DeviceAddress ds4addr = {0x28, 0xFF, 0x4F, 0xAB, 0x6E, 0x18, 0x01, 0x66};

  t1 = new Supla::Sensor::DS18B20(23, ds1addr);  // Channel #0
  t2 = new Supla::Sensor::DS18B20(23, ds2addr);  // Channel #1
  t3 = new Supla::Sensor::DS18B20(23, ds3addr);  // Channel #2
  t4 = new Supla::Sensor::DS18B20(23, ds4addr);  // Channel #3

  // Register notifications
  Supla::Notification::RegisterNotification(-1);  // notifications for device
  Supla::Notification::RegisterNotification(0);  // notifications for channel 0
  Supla::Notification::RegisterNotification(1);  // notifications for channel 1
  // There are extra flags that can tell Supla that some fields can be filled by
  // user on Supla Cloud side.
  // First bool flag tells whether "title" is set by device (true) or server
  // (false)
  // Second bool flag - "message body" is set by device (true) or server (false)
  // I.e.:
  // Supla::Notification::RegisterNotification(2, false, true);
  // notifications for channel 2 with title set by server and message body
  // set by device

  /*
   SuplaDevice Initialization. Server address is available at
   https://cloud.supla.org
   If you do not have an account, you can create it at
   https://cloud.supla.org/account/create
   SUPLA and SUPLA CLOUD are free of charge
  */

  SuplaDevice.begin(
      GUID,              // Global Unique Identifier
      "svr1.supla.org",  // SUPLA server address
      "email@address",   // Email address used to login to Supla Cloud
      AUTHKEY);          // Authorization key
}

void loop() {
  SuplaDevice.iterate();

  static uint32_t lastTime = 0;
  if (millis() - lastTime > 60000) {
    lastTime = millis();
    static bool greetingsSend = false;
    if (!greetingsSend) {
      greetingsSend = true;
      Supla::Notification::Send(
          -1, "The Device", "Hello World! I'm your device with notifications!");
    }

    Supla::Notification::SendF(0, "Home", "Current temperature %0.2f",
        t1->getChannel()->getValueDouble());
    if (t2->getChannel()->getValueDouble() < 5) {
      Supla::Notification::Send(1, "Outside", "It is cold outside!");
    }
  }
}
