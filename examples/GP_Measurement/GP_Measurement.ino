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

// Please adjust GPIO to your board configuration
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
#include <supla/device/supla_ca_cert.h>
#include <supla/sensor/general_purpose_measurement.h>

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

// HTML www component (they appear in sections according to creation
// sequence)
Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;

Supla::Sensor::GeneralPurposeMeasurement *gpm = nullptr;

void setup() {
  Serial.begin(115200);

  // Channels configuration
  // Channel #0 General Purpose Measurement
  // GPM class by deafult works as "virtual channel" (it accepts any value
  // via setValue(double) method.
  gpm = new Supla::Sensor::GeneralPurposeMeasurement();

  // Default channel config values are initialized only once. They can be
  // modified later, but they won't change corresponding used configurable
  // parameters.
  // Below lines are optional, just remove them if you don't want to set
  // them.
  gpm->setDefaultValueDivider(0);  // in 0.001 steps, 0 is ignored
  gpm->setDefaultValueMultiplier(0);  // in 0.001 steps, 0 is ignored
  gpm->setDefaultValueAdded(0);  // in 0.001 steps
  gpm->setDefaultUnitBeforeValue("before");
  gpm->setDefaultUnitAfterValue("after");
  gpm->setDefaultValuePrecision(3);  // 0..4 - number of decimal places

  // Set some initial value of measurement
  gpm->setValue(3.1415);

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();

  static uint32_t lastTime = 0;
  if (millis() - lastTime > 1000) {
    lastTime = millis();
    // set some new value on gpm:
    gpm->setValue(millis() / 1000.0);
  }
}

