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

/* This example shows ESP82xx/ESP32 based device with simple web interface
 * used to configure Wi-Fi parameters and Supla server connection.
 * Additionally it shows how to use some of HTML elements provided by
 * supla-device library.
 *
 * After fresh installation, device will be in config mode. It will have its
 * own Wi-Fi AP configured. You should connect to it with your mobile phone
 * and open http://192.168.4.1 where you can configure the device.
 * Status LED is also configured. Please adjust GPIOs to your HW.
 */

#define STATUS_LED_GPIO 2
#define RELAY_GPIO 12
#define BUTTON_CFG_RELAY_GPIO 0

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/relay.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/events.h>

// Includes for HTML elements
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/html/custom_parameter.h>
#include <supla/network/html/custom_text_parameter.h>
#include <supla/network/html/text_cmd_input_parameter.h>
#include <supla/network/html/select_cmd_input_parameter.h>

// Choose where Supla should store roller shutter data in persistent memory
// We recommend to use external FRAM memory
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(STORAGE_OFFSET);

// This class provides Wi-Fi network connectivity for your device
Supla::ESPWifi wifi;

// This class provides web server to handle config mode
Supla::EspWebServer suplaServer;

// This class provides configuration storage (like SSID, password, all
// user defined parameters, etc. We use LittleFS.
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state


// Those tags are used for HTML element names and for keys to access parameter
// values in Config storage class. Max length of those values is 15 chars.
const char PARAM1[] = "param1";
const char PARAM2[] = "param2";
const char PARAM_CMD1[] = "cmd1";
const char PARAM_CMD2[] = "cmd2";

void setup() {

  Serial.begin(115200);
  // Channels configuration
  // CH 0 - Relay
  auto r1 = new Supla::Control::Relay(RELAY_GPIO);
  // CH 1 - Action trigger
  auto at1 = new Supla::Control::ActionTrigger();

  // Buttons configuration
  auto buttonCfgRelay =
    new Supla::Control::Button(BUTTON_CFG_RELAY_GPIO, true, true);

  buttonCfgRelay->configureAsConfigButton(&SuplaDevice);
  buttonCfgRelay->addAction(Supla::TOGGLE, r1, Supla::ON_CLICK_1);

  // Action trigger configuration
  at1->setRelatedChannel(r1);
  at1->attach(buttonCfgRelay);

  // configure defualt Supla CA certificate
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

  // HTML www component (they appear in sections according to creation
  // sequence).
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // Here user defined inputs are defined.
  // Simple text input:
  // PARAM1 - input name and key used for storage in Config instance
  // "Please provide text value" - label for field on config page
  // 100 - maximum text length accepted by your input
  new Supla::Html::CustomTextParameter(
      PARAM1, "Please provide text value", 100);

  // Numeric input:
  // PARAM2 - input name and key used for storage in Config instance
  // "Your age" - label for field on config page
  // 100 - maximum text length accepted by your input
  new Supla::Html::CustomParameter(PARAM2, "Your age");

  // Text based command input
  // PARAM_CMD1 - Html field input name
  // "Relay control" - label that is displayed on config page
  auto textCmd =
    new Supla::Html::TextCmdInputParameter(PARAM_CMD1, "Relay control");
  // First we register allowed text input values and we assign which event
  // should be generated:
  textCmd->registerCmd("ON", Supla::ON_EVENT_1);
  textCmd->registerCmd("OFF", Supla::ON_EVENT_2);
  textCmd->registerCmd("TOGGLE", Supla::ON_EVENT_3);
  // Then we link events with actions on a relay.
  // Last "true" parameter will make sure that those actions won't be disabled
  // when we enter config mode. By default actions are disabled when device
  // enters config mode, but those text commands can be used only in config
  // mode, so we want to have them enabled.
  textCmd->addAction(Supla::TURN_ON, r1, Supla::ON_EVENT_1, true);
  textCmd->addAction(Supla::TURN_OFF, r1, Supla::ON_EVENT_2, true);
  textCmd->addAction(Supla::TOGGLE, r1, Supla::ON_EVENT_3, true);

  // Select based command input - exactly the same configuration as for text
  // field
  // PARAM_CMD2 - Html field input name
  // "Relay control" - label that is displayed on config page
  auto selectCmd =
    new Supla::Html::SelectCmdInputParameter(PARAM_CMD1, "Relay control");
  // First we register allowed text input values and we assign which event
  // should be generated:
  selectCmd->registerCmd("ON", Supla::ON_EVENT_1);
  selectCmd->registerCmd("OFF", Supla::ON_EVENT_2);
  selectCmd->registerCmd("TOGGLE", Supla::ON_EVENT_3);
  // Then we link events with actions on a relay.
  // Last "true" parameter will make sure that those actions won't be disabled
  // when we enter config mode. By default actions are disabled when device
  // enters config mode, but those text commands can be used only in config
  // mode, so we want to have them enabled.
  selectCmd->addAction(Supla::TURN_ON, r1, Supla::ON_EVENT_1, true);
  selectCmd->addAction(Supla::TURN_OFF, r1, Supla::ON_EVENT_2, true);
  selectCmd->addAction(Supla::TOGGLE, r1, Supla::ON_EVENT_3, true);

  // Set custom device name
  SuplaDevice.setName("SUPLA-INPUT-EXAMPLE");
  // Start!
  SuplaDevice.begin();

  // Below lines show how to access values provided by user via config mode.
  // It has to be used after SuplaDevice.begin() or in loop(). If you would
  // like to access them before begin(), then please first call:
  // Supla::Storage::Init();
  char buf[200] = {};
  if (Supla::Storage::ConfigInstance()->getString(PARAM1, buf, 200)) {
    SUPLA_LOG_DEBUG(" **** Param[%s]: %s", PARAM1, buf);
  } else {
    SUPLA_LOG_DEBUG(" **** Param[%s] is not set", PARAM1);
  }
  int32_t intValue = 0;
  if (Supla::Storage::ConfigInstance()->getInt32(PARAM2, &intValue)) {
    SUPLA_LOG_DEBUG(" **** Param[%s]: %d", PARAM2, intValue);
  } else {
    SUPLA_LOG_DEBUG(" **** Param[%s] is not set", PARAM2);
  }
}

void loop() {
  SuplaDevice.iterate();
}

