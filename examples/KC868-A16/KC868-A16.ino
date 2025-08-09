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
*/

constexpr uint8_t SDA_ = 4;
constexpr uint8_t SCL_ = 5;
constexpr uint8_t CFG_BTN_GPIO = 14;
constexpr uint8_t MAX_CHANNELS = 16;

#include <SuplaDevice.h>

#include <supla/network/esp_wifi.h>
Supla::ESPWifi *wifi_ = nullptr;
#include <supla/network/esp32eth.h>
Supla::ESPETH *eth_ = nullptr;

#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;
#include <supla/storage/littlefs_config.h>
Supla::LittleFsConfig configSupla(4096);

#include <supla/network/esp_web_server.h>
Supla::EspWebServer suplaServer;
#include <HTTPUpdateServer.h>
HTTPUpdateServer httpUpdater;

#include <supla/io/PCF8574.h>
Supla::Io::PCF8574 *inpcf_[2] = {};
Supla::Io::PCF8574 *outpcf_[2] = {};

#include <supla/control/button.h>
Supla::Control::Button *button_[MAX_CHANNELS] = {};

#include <supla/control/action_trigger.h>
Supla::Control::ActionTrigger *at_[MAX_CHANNELS] = {};

#include <supla/control/relay.h>
Supla::Control::Relay *relay_[MAX_CHANNELS] = {};

#include <supla/control/virtual_relay.h>
Supla::Control::VirtualRelay *websrv_ = nullptr;

#include <supla/device/supla_ca_cert.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/network/html/ethernet_parameters.h>
#include <supla/network/html/text_cmd_input_parameter.h>

class CustomIterate : public Supla::Element {
 public: CustomIterate() {}
  void iterateAlways() override {
    if (Supla::Network::IsReady()) {
      if (websrv_->isOn() && !srvStarted) {
        SuplaDevice.handleAction(0, Supla::START_LOCAL_WEB_SERVER);
        srvStarted = true;
      } else if (!websrv_->isOn() && srvStarted) {
        SuplaDevice.handleAction(0, Supla::STOP_LOCAL_WEB_SERVER);
        srvStarted = false;
      }
    }
  }
 protected:
  bool srvStarted = false;
};
CustomIterate customIterate;

void setup() {

  Serial.begin(115200);
  Wire.begin(SDA_, SCL_, 400'000);

  wifi_ = new Supla::ESPWifi;
  eth_ = new Supla::ESPETH(0);

  auto buttonCfg = new Supla::Control::Button(CFG_BTN_GPIO, true, true);
  buttonCfg->configureAsConfigButton(&SuplaDevice);

  websrv_ = new Supla::Control::VirtualRelay(32);
  websrv_->getChannel()->setChannelNumber(0);
  websrv_->setInitialCaption("WebSrv");
  websrv_->setDefaultStateRestore();
  websrv_->disableCountdownTimerFunction();
  websrv_->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);

  for (int i = 0; i < 2; i++) {
    inpcf_[i] = new Supla::Io::PCF8574(0x21+i, 0xFF);
    outpcf_[i] = new Supla::Io::PCF8574(0x24+i, 0xFF);
  }

  constexpr uint8_t gpio_[] = {0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7};

  for (int i = 0; i < MAX_CHANNELS; i++) {
    uint8_t pcfIndex = (i < MAX_CHANNELS/2) ? 0 : 1;
    relay_[i] = new Supla::Control::Relay(
                                      outpcf_[pcfIndex], gpio_[i], false, 224);
    relay_[i]->getChannel()->setDefault(SUPLA_CHANNELFNC_POWERSWITCH);
    char buff[15];
    if (i+1 < 10) {
      snprintf(buff, sizeof(buff), "RELAY - 0%d", i+1);
    } else {
      snprintf(buff, sizeof(buff), "RELAY - %d", i+1);
    }
    relay_[i]->setInitialCaption(buff);
    relay_[i]->getChannel()->setChannelNumber((i*2)+1+10);
    relay_[i]->disableChannelState();
    relay_[i]->setDefaultStateRestore();
    button_[i] = new Supla::Control::Button(
                                       inpcf_[pcfIndex], gpio_[i], true, true);
    button_[i]->setHoldTime(400);
    button_[i]->setMulticlickTime(350);
    button_[i]->addAction(Supla::TOGGLE, relay_[i], Supla::ON_CLICK_1);
    at_[i] = new Supla::Control::ActionTrigger();
    at_[i]->getChannel()->setChannelNumber((i*2)+2+10);
    at_[i]->setRelatedChannel(relay_[i]);
    at_[i]->attach(button_[i]);
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

  httpUpdater.setup(suplaServer.getServerPtr(), "/update");
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);
  SuplaDevice.setCustomHostnamePrefix("SUPLA-A16");
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.allowWorkInOfflineMode(0);
  SuplaDevice.setName("KC868-A16");
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
