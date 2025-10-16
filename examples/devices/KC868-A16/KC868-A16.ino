/*
  Copyright (C) lukfud

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

  ------------------------------------------------------------------------------------------------
  The following sketch allows for basic operation of the KC868-A16 device using PCF8574 expanders.
  It creates 16 relays, 16 buttons, and 16 action triggers.
  https://www.kincony.com/esp32-board-16-channel-relay-hardware.html

*/

constexpr uint8_t SDA_ = 4;
constexpr uint8_t SCL_ = 5;
constexpr uint8_t CFG_BTN_GPIO = 14;  // GPIO termianl T3
constexpr uint8_t MAX_CHANNELS = 16;

#include <SuplaDevice.h>

#include <supla/network/esp_wifi.h>
Supla::ESPWifi *wifi = nullptr;
#include <supla/network/esp32eth.h>
Supla::ESPETH *eth = nullptr;

#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
#include <supla/storage/littlefs_config.h>
Supla::LittleFsConfig configSupla(4096);

#include <supla/network/esp_web_server.h>
Supla::EspWebServer suplaServer;

#include <supla/io/PCF8574.h>
Supla::Io::PCF8574 *inPcf[2] = {};
Supla::Io::PCF8574 *outPcf[2] = {};

#include <supla/control/button.h>
Supla::Control::Button *button[MAX_CHANNELS] = {};

#include <supla/control/action_trigger.h>
Supla::Control::ActionTrigger *at[MAX_CHANNELS] = {};

#include <supla/control/relay.h>
Supla::Control::Relay *relay[MAX_CHANNELS] = {};

#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/html/ethernet_parameters.h>
#include <supla/network/html/text_cmd_input_parameter.h>

void setup() {
  Serial.begin(115'200);
  Wire.begin(SDA_, SCL_, 400'000);

  wifi = new Supla::ESPWifi;
  eth = new Supla::ESPETH(0);

  auto cfgButton = new Supla::Control::Button(CFG_BTN_GPIO, true, true);
  cfgButton->configureAsConfigButton(&SuplaDevice);

  for (int i = 0; i < 2; i++) {
    inPcf[i] = new Supla::Io::PCF8574(0x22 - i);
    outPcf[i] = new Supla::Io::PCF8574(0x24 + i);
  }

  constexpr uint8_t GPIO[] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    0, 1, 2, 3, 4, 5, 6, 7
  };

  for (int i = 0; i < MAX_CHANNELS; i++) {
    uint8_t pcfIndex = (i < MAX_CHANNELS / 2) ? 0 : 1;
    relay[i] = new Supla::Control::Relay(outPcf[pcfIndex], GPIO[i], false, 224);
    relay[i]->getChannel()->setDefault(SUPLA_CHANNELFNC_LIGHTSWITCH);
    char buff[15];
    snprintf(buff, sizeof(buff), "RELAY - %02d", i + 1);
    relay[i]->setInitialCaption(buff);
    relay[i]->getChannel()->setChannelNumber((i * 2) + 1 + 10);
    relay[i]->disableChannelState();
    relay[i]->setDefaultStateRestore();
    button[i] = new Supla::Control::Button(
                                          inPcf[pcfIndex], GPIO[i], true, true);
    button[i]->setHoldTime(400);
    button[i]->setMulticlickTime(350);
    button[i]->addAction(Supla::TOGGLE, relay[i], Supla::ON_CLICK_1);
    at[i] = new Supla::Control::ActionTrigger();
    at[i]->getChannel()->setChannelNumber((i * 2) + 2 + 10);
    at[i]->setRelatedChannel(relay[i]);
    at[i]->attach(button[i]);
  }

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::EthernetParameters;
  new Supla::Html::ProtocolParameters(false, false);
  auto textCmd = new Supla::Html::TextCmdInputParameter(
                                                    "cmd1", "Reset to factory");
  textCmd->registerCmd("RESET", Supla::ON_EVENT_1);
  textCmd->addAction(Supla::RESET_TO_FACTORY_SETTINGS,
                                                SuplaDevice, Supla::ON_EVENT_1);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartWithCfgModeThenOffline);
  SuplaDevice.setCustomHostnamePrefix("SUPLA-A16");
  SuplaDevice.setName("KC868-A16");
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
