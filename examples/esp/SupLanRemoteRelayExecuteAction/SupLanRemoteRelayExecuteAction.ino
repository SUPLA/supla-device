// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file SupLanRemoteRelayExecuteAction.ino
 * @brief Control a remote relay from a local VirtualRelay using executeAction.
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
constexpr int REMOTE_TOGGLE_BUTTON_GPIO = 4;

Supla::ChannelHandle remoteRelay(REMOTE_RELAY_CHANNEL_ID, Supla::CONTROL);
Supla::Control::VirtualRelay commandRelay(SUPLA_BIT_FUNC_POWERSWITCH);

class RemoteRelayCommands : public Supla::ActionHandler {
 public:
  void handleAction(int, int action) override {
    if (action != Supla::TURN_ON && action != Supla::TURN_OFF &&
        action != Supla::TOGGLE) return;
    if (!remoteRelay.hasControlPermission()) {
      Serial.println("Remote relay CONTROL is not provisioned");
      return;
    }
    // Network ACTION_* IDs differ from the local Supla::Action enum.
    const uint32_t actionId = action == Supla::TOGGLE ? ACTION_TOGGLE
        : action == Supla::TURN_ON ? ACTION_TURN_ON : ACTION_TURN_OFF;
    const bool admitted = remoteRelay.executeAction(actionId);
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

  auto toggleButton =
      new Supla::Control::Button(REMOTE_TOGGLE_BUTTON_GPIO, true, true);
  toggleButton->addAction(Supla::TOGGLE, remoteCommands, Supla::ON_CLICK_1);

  commandRelay.setDefaultStateOff();
  commandRelay.addAction(Supla::TURN_ON, remoteCommands, Supla::ON_TURN_ON);
  commandRelay.addAction(Supla::TURN_OFF, remoteCommands, Supla::ON_TURN_OFF);
  // A short click or a command from Cloud changes the local VirtualRelay.
  configButton->addAction(Supla::TOGGLE, commandRelay, Supla::ON_CLICK_1);

  SuplaDevice.setName("SupLAN RemoteRelayExecuteAction");
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
