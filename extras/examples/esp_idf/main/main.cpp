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

/*
 Example of supla-device project for ESP32 with EPS-IDF SDK
 */

#include <SuplaDevice.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <supla/log_wrapper.h>
#include <supla/control/button.h>
#include <supla/control/relay.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/virtual_relay.h>
#include <supla/device/status_led.h>
#include <supla/time.h>

// Supla extras/porting/esp-idf files - specific to ESP-IDF and ESP8266 RTOS
// targets
#include <esp_idf_web_server.h>
#include <esp_idf_wifi.h>
#include <nvs_config.h>
#include <spiffs_storage.h>

// HTML generation
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>

extern "C" void cpp_main(void*);

extern const uint8_t suplaOrgCertPemStart[] asm(
    "_binary_supla_org_cert_pem_start");

extern const uint8_t supla3rdCertPemStart[] asm(
    "_binary_supla_3rd_cert_pem_start");

void cpp_main(void* param) {
  // Content of HTML for cfg mode
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Device status LED
  // Supla::Device::StatusLed statusLed(2, false); // esp-wroom-32 GPIO2
  new Supla::Device::StatusLed(18, false);  // esp-wroom-32-c3 GPIO18

  // Spiffs storage for channel states
  new Supla::SpiffsStorage(512);

  // Nvs based device configuration storage
  new Supla::NvsConfig;

  // Cfg mode web server
  new Supla::EspIdfWebServer;

  // Show all SUPLA related logs
  esp_log_level_set("SUPLA", ESP_LOG_DEBUG);

  Supla::EspIdfWifi wifi;

  auto r1 = new Supla::Control::VirtualRelay();
  auto r2 = new Supla::Control::VirtualRelay();
#define RELAY3_GPIO 8
  auto r3 = new Supla::Control::Relay(RELAY3_GPIO);

  new Supla::Control::RollerShutter(12, 13, true);

  auto b1 = new Supla::Control::Button(7, true, true);

  b1->setHoldTime(1000);
  b1->setMulticlickTime(300);
  b1->addAction(Supla::TOGGLE, r1, Supla::ON_CLICK_1);
  b1->addAction(Supla::START_LOCAL_WEB_SERVER, SuplaDevice, Supla::ON_CLICK_2);
  b1->addAction(Supla::STOP_LOCAL_WEB_SERVER, SuplaDevice, Supla::ON_CLICK_3);
  b1->addAction(
      Supla::RESET_TO_FACTORY_SETTINGS, SuplaDevice, Supla::ON_CLICK_5);
  b1->addAction(Supla::TOGGLE_CONFIG_MODE, SuplaDevice, Supla::ON_HOLD);

  r1->setDefaultStateRestore();
  r2->setDefaultStateRestore();
  r3->setDefaultStateRestore();

  SUPLA_LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));

  SuplaDevice.setSuplaCACert(
      reinterpret_cast<const char*>(suplaOrgCertPemStart));
  SuplaDevice.setSupla3rdPartyCACert(
      reinterpret_cast<const char*>(supla3rdCertPemStart));


  SuplaDevice.begin();
  SUPLA_LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  SUPLA_LOG_DEBUG("port tick period %d", portTICK_PERIOD_MS);

  unsigned int lastTime = 0;
  unsigned int lastTimeHeap = 0;
  int lastFreeHeap = 0;

  while (true) {
    SuplaDevice.iterate();
    if (millis() - lastTime > 10) {
      lastTime = millis();
      delay(1);
    }

    // debug logs with free heap (TODO: remove)
    if (millis() - lastTimeHeap > 2000) {
      int curHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      if (lastFreeHeap != curHeap) {
        lastFreeHeap = curHeap;
        lastTimeHeap = millis();
        SUPLA_LOG_DEBUG("Free heap: %d",
                  heap_caps_get_free_size(MALLOC_CAP_8BIT));
      }
    }
  }
}

extern "C" {
void app_main() {
  xTaskCreate(&cpp_main, "cpp_main", 8192, NULL, 5, NULL);
}
}
