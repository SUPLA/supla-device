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
 * @file GPM_EspFreeHeap.ino
 * @brief Example of monitoring ESP8266/ESP32 free heap memory as a General Purpose Measurement (GPM) for SUPLA.
 * This example demonstrates how to create virtual GPM sensors to display the available free heap memory
 * of an ESP device in both bytes and kilobytes within the SUPLA cloud.
 * It uses Wi-Fi for network connectivity and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * A status LED is also configured.
 *
 * @tags GPM, EspFreeHeap, memory, heap, monitor, esp, esp32, esp8266, wifi, web_interface
 */

#define STATUS_LED_GPIO 2

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>

#include <supla/sensor/gpm_esp_free_heap.h>

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

  // Channels configuration
  // Channel #0 Free heap memory on ESP in B
  auto freeHeap = new Supla::Sensor::GpmEspFreeHeap();

  // Channel #1 Free heap memory on ESP in KB
  auto freeHeapKB = new Supla::Sensor::GpmEspFreeHeap();
  freeHeapKB->setDefaultUnitAfterValue("KB");
  freeHeapKB->setDefaultValueDivider(1024000);  // in 0.001 units

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
