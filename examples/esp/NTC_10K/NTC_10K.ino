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

/* This example shows ESP82xx/ESP32 based device with simple WebInterface
 * used to configure Wi-Fi parameters and Supla server connection.
 * There is one RollerShutter, one Relay and 3 buttons configured.
 * Two buttons are for roller shutter with Action Trigger.
 * Third button is for controlling the relay and for switching module to
 * config mode.
 * After fresh installation, device will be in config mode. It will have its
 * own Wi-Fi AP configured. You should connect to it with your mobile phone
 * and open http://192.168.4.1 where you can configure the device.
 * Status LED is also configured. Please adjust GPIOs to your HW.
 */


/* 
Connection:

    +3.3V
      |
    __|__
    |    |
    |    |  10K
    |    |
    |____|
      |
      | ---() Analog IN
      |
    __|__
    |    |
    |    |  Thermistor NTC10k
    |    |
    |____|  
      |
      |
    ______ GND

    This example works with NTC100K too

 */
 
#define STATUS_LED_GPIO 2
#define NTC_SENSOR_PIN 36
#define BUTTON_CFG_RELAY_GPIO 0
#define NTC_RESISTANCE 10000
#define RESISTOR_RESISTANCE 10000
#define NOMINAL_TEMP 25
#define BETA_ 3950


#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/roller_shutter.h>
#include <supla/control/relay.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/sensor/ntc10k.h>

#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;

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

  new Supla::Sensor::NTC10k(NTC_SENSOR_PIN, RESISTOR_RESISTANCE, NTC_RESISTANCE, NOMINAL_TEMP, BETA_);

  auto buttonCfgRelay = new Supla::Control::Button(BUTTON_CFG_RELAY_GPIO, true, true);

  buttonCfgRelay->configureAsConfigButton(&SuplaDevice);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
