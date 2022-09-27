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
 Example of supla-device project for ESP8266 with EPS8266 RTOS SDK
 */

#include <FreeRTOS.h>
#include <SuplaDevice.h>
#include <esp_heap_caps.h>
#include <esp_idf_web_server.h>
#include <esp_idf_wifi.h>
#include <nvs_config.h>
#include <spiffs_storage.h>
#include <supla/log_wrapper.h>
#include <supla/control/button.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/virtual_relay.h>
#include <supla/device/status_led.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/time.h>

extern "C" void cpp_main(void*);

extern const uint8_t suplaOrgCertPemStart[] asm(
    "_binary_supla_org_cert_pem_start");

void cpp_main(void* param) {
  new Supla::EspIdfWifi;
  new Supla::SpiffsStorage(512);
  new Supla::NvsConfig;
  new Supla::Device::StatusLed(2, true);  // nodemcu GPIO2, inverted state
  new Supla::EspIdfWebServer;

  // HTML www component (they appear in sections according to creation
  // sequence)
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  auto r1 = new Supla::Control::Relay(12);
  auto r2 = new Supla::Control::VirtualRelay();
  auto b1 = new Supla::Control::Button(14, true, true);
  b1->setHoldTime(1000);
  b1->setMulticlickTime(300);
  b1->addAction(Supla::TOGGLE, r1, Supla::ON_CLICK_1);
  b1->addAction(Supla::START_LOCAL_WEB_SERVER, SuplaDevice, Supla::ON_CLICK_2);
  b1->addAction(
      Supla::RESET_TO_FACTORY_SETTINGS, SuplaDevice, Supla::ON_CLICK_5);
  b1->addAction(Supla::TOGGLE_CONFIG_MODE, SuplaDevice, Supla::ON_HOLD);

  SUPLA_LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
  SuplaDevice.setName("SUPLA-Example");
  SuplaDevice.setSuplaCACert(
      reinterpret_cast<const char*>(suplaOrgCertPemStart));
  SuplaDevice.begin();
  SUPLA_LOG_DEBUG("Free heap: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));

  unsigned int lastTime = 0;
  unsigned int lastTimeHeap = 0;
  SUPLA_LOG_DEBUG("Starting main loop");
  while (true) {
    SuplaDevice.iterate();
    if (millis() - lastTime > 10) {
      delay(1);
      lastTime = millis();
    }
    if (millis() - lastTimeHeap > 3000) {
      lastTimeHeap = millis();
      static int lastFreeHeap = 0;
      int curHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      if (lastFreeHeap != curHeap) {
        lastFreeHeap = curHeap;
        SUPLA_LOG_DEBUG("Free heap: %d", lastFreeHeap);
      }
    }
  }
}

extern "C" {
void app_main() {
  xTaskCreate(&cpp_main, "cpp_main", 8192, NULL, 1, NULL);
}
}
