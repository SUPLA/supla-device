// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file SupLanRemoteRelaySetValue.ino
 * @brief Control a remote relay from a local VirtualRelay using setValue.
 * ESP8266/ESP32; Wi-Fi and SUPLA settings use the web configuration interface.
 * Serial is output-only logging. See README.md for approval and wiring.
 * @tags suplan, remote_channel, web_interface, esp8266, esp32
 */

#include <SuplaDevice.h>
#include <supla/channels/channel_handle.h>
#include <supla/control/virtual_relay.h>
#include <supla/control/button.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>
#include <supla/events.h>

constexpr int CONFIG_BUTTON_GPIO = 0;
constexpr int STATUS_LED_GPIO = 2;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;
Supla::EspWebServer webServer;
Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);
bool deviceStarted = false;

// Global ChannelId of the Source power/light relay, not its ChannelNumber.
constexpr uint32_t REMOTE_RELAY_CHANNEL_ID = 12345;

Supla::ChannelHandle remoteRelay(REMOTE_RELAY_CHANNEL_ID, Supla::CONTROL);
Supla::Control::VirtualRelay commandRelay(SUPLA_BIT_FUNC_POWERSWITCH);

class RemoteRelayCommands : public Supla::ActionHandler {
 public:
  void handleAction(int, int action) override {
    if (action != Supla::TURN_ON && action != Supla::TURN_OFF) return;
    if (!remoteRelay.hasControlPermission()) {
      Serial.println("Remote relay CONTROL is not provisioned");
      return;
    }
    uint8_t value[8] = {};
    value[0] = action == Supla::TURN_ON ? 1 : 0;
    const bool admitted = remoteRelay.setValue(value);
    Serial.println(admitted ? "Remote command admitted locally"
                            : "Remote command rejected locally");
  }
};
RemoteRelayCommands remoteCommands;

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;

  auto configButton =
      new Supla::Control::Button(CONFIG_BUTTON_GPIO, true, true);
  configButton->configureAsConfigButton(&SuplaDevice);

  commandRelay.setDefaultStateOff();
  commandRelay.addAction(Supla::TURN_ON, remoteCommands, Supla::ON_TURN_ON);
  commandRelay.addAction(Supla::TURN_OFF, remoteCommands, Supla::ON_TURN_OFF);
  // A short click or a command from Cloud changes the local VirtualRelay.
  configButton->addAction(Supla::TOGGLE, commandRelay, Supla::ON_CLICK_1);

  SuplaDevice.setName("SupLAN RemoteRelaySetValue");
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  deviceStarted = SuplaDevice.enableSupLan() && SuplaDevice.begin(29);
  if (!deviceStarted) Serial.println("Device/SupLAN initialization failed");
}

void loop() {
  if (!deviceStarted) return;
  SuplaDevice.iterate();

  static int previousStatus = -1;
  const int status = static_cast<int>(remoteRelay.accessStatus());
  if (status != previousStatus) {
    Serial.printf("Remote access status: %d\n", status);
    previousStatus = status;
  }
}
