/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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
 * @file three_phase_PzemV3_ADDR.ino
 * @brief Example of connecting multiple (three-phase) Peacefair PZEM-004T V3 energy monitors with unique addresses to SUPLA using an ESP8266/ESP32.
 * This example demonstrates how to integrate three PZEM-004T V3 energy monitors, each configured with a distinct Modbus address,
 * with an ESP device. It sends their readings (voltage, current, power, energy for each phase) to the SUPLA cloud via Wi-Fi.
 * It includes a web server for configuration.
 * Network settings are configured via the web interface.
 * Users need to adjust the RX/TX pins for the PZEM modules, along with their respective Modbus addresses in the code.
 * A status LED is also configured.
 *
 * @dependency https://github.com/mandulaj/PZEM-004T-v30
 * @tags PZEM, PZEM-004T, V3, three_phase, energy, monitor, voltage, current, power, esp, esp32, esp8266, wifi, web_interface, modbus
 */

#include <SuplaDevice.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/sensor/three_phase_PzemV3_ADDR.h>
#include <supla/storage/littlefs_config.h>

#define Pzem_RX         32
#define Pzem_TX         33
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

  // TODO(anyone): fix gpio settings/variant for ESP32 or ESP8266
  // ESP32  ThreePhasePZEMv3_ADDR(Serial prot, PZEM RX PIN, PZEM TX PIN, PZEM 1
  // Addr, PZEM 2 Addr, PZEM 3 Addr)
  new Supla::Sensor::ThreePhasePZEMv3_ADDR(
      &Serial2, Pzem_RX, Pzem_TX, 0xAA, 0xAB, 0xAC);

  // ESP32  ThreePhasePZEMv3_ADDR(Serial prot, PZEM RX PIN, PZEM TX PIN) "PZEM
  // 1-3 Addr default to 0x01, 0x02, 0x03" new
  // Supla::Sensor::ThreePhasePZEMv3_ADDR(&Serial2, Pzem_RX, Pzem_TX);

  // ESP8266  ThreePhasePZEMv3_ADDR(PZEM RX PIN, PZEM TX PIN, PZEM 1 Addr, PZEM
  // 2 Addr, PZEM 3 Addr) new Supla::Sensor::ThreePhasePZEMv3_ADDR( Pzem_RX,
  // Pzem_TX, 0xAA, 0xAB, 0xAC);

  // ESP8266  ThreePhasePZEMv3_ADDR(PZEM RX PIN, PZEM TX PIN) "PZEM 1-3 Addr
  // default to 0x01, 0x02, 0x03" new Supla::Sensor::ThreePhasePZEMv3_ADDR(
  // Pzem_RX, Pzem_TX);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
