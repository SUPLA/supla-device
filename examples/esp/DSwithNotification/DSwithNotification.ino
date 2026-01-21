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
 * @file DSwithNotification.ino
 * @brief Example of connecting multiple DS18B20 temperature sensors to SUPLA with notifications using an ESP8266/ESP32.
 * This example extends the basic DS18B20 sensor setup by demonstrating how to send notifications
 * to the SUPLA cloud based on sensor readings. It configures an ESP device with Wi-Fi
 * to read data from multiple DS18B20 sensors and integrate it with SUPLA, sending alerts (e.g., for low temperatures).
 * It includes a web server for Wi-Fi and SUPLA server configuration.
 * Network settings are configured via the web interface.
 * Users may need to adjust sensor addresses and notification logic in the code.
 *
 * @dependency https://github.com/milesburton/Arduino-Temperature-Control-Library
 * @tags DS18B20, DallasTemperature, temperature, sensor, onewire, notifications, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/device/notifications.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/DS18B20.h>
#include <supla/storage/littlefs_config.h>
#include <supla/time.h>

#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

Supla::Sensor::DS18B20 *t1 = nullptr;
Supla::Sensor::DS18B20 *t2 = nullptr;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // 2 DS18B20 thermometers at pin 23. DS address can be omitted when there is
  // only one device at a pin

  // TODO(anyone): Add HTML elements to configure DS18B20 addresses, with
  // scan etc.
  DeviceAddress ds1addr = {0x28, 0xFF, 0xC8, 0xAB, 0x6E, 0x18, 0x01, 0xFC};
  DeviceAddress ds2addr = {0x28, 0xFF, 0x54, 0x73, 0x6E, 0x18, 0x01, 0x77};

  t1 = new Supla::Sensor::DS18B20(23, ds1addr);  // Channel #0
  t2 = new Supla::Sensor::DS18B20(23, ds2addr);  // Channel #1

  // Register notifications
  Supla::Notification::RegisterNotification(-1);  // notifications for device
  Supla::Notification::RegisterNotification(0);   // notifications for channel 0
  Supla::Notification::RegisterNotification(1);   // notifications for channel 1

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
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

    Supla::Notification::SendF(0,
                               "Home",
                               "Current temperature %0.2f",
                               t1->getChannel()->getValueDouble());
    if (t2->getChannel()->getValueDouble() < 5) {
      Supla::Notification::Send(1, "Outside", "It is cold outside!");
    }
  }
}
