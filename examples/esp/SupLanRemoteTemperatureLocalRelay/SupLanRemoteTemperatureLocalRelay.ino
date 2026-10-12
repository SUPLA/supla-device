// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file SupLanRemoteTemperatureLocalRelay.ino
 * @brief Use remote temperature conditions to control a local relay.
 * ESP8266/ESP32; Wi-Fi and SUPLA settings use the web configuration interface.
 * Serial is output-only logging. See README.md for approval and wiring.
 * @tags suplan, remote_channel, web_interface, esp8266, esp32
 */

#include <SuplaDevice.h>
#include <supla/channels/remote_channel.h>
#include <supla/condition.h>
#include <supla/control/relay.h>
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

// Global ChannelId of the Source thermometer, not its ChannelNumber.
constexpr uint32_t REMOTE_THERMOMETER_CHANNEL_ID = 12345;
constexpr int RELAY_GPIO = 5;
constexpr double TURN_ON_BELOW_C = 20.0;
constexpr double TURN_OFF_ABOVE_C = 22.0;

Supla::RemoteChannel thermometer(REMOTE_THERMOMETER_CHANNEL_ID, Supla::READ);
Supla::Control::Relay relay(RELAY_GPIO, true, SUPLA_BIT_FUNC_POWERSWITCH);

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;

  auto configButton =
      new Supla::Control::Button(CONFIG_BUTTON_GPIO, true, true);
  configButton->configureAsConfigButton(&SuplaDevice);

  relay.setDefaultStateOff();
  thermometer.addAction(Supla::TURN_ON, relay, Supla::ON_CHANGE,
                        OnLess(TURN_ON_BELOW_C));
  thermometer.addAction(Supla::TURN_OFF, relay, Supla::ON_CHANGE,
                        OnGreater(TURN_OFF_ABOVE_C));

  SuplaDevice.setName("SupLAN RemoteTemperatureLocalRelay");
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  deviceStarted = SuplaDevice.enableSupLan() && SuplaDevice.begin(29);
  if (!deviceStarted) Serial.println("Device/SupLAN initialization failed");
}

void loop() {
  if (!deviceStarted) return;
  SuplaDevice.iterate();

  static int previousStatus = -1;
  const int status = static_cast<int>(thermometer.accessStatus());
  if (status != previousStatus) {
    Serial.printf("Remote access status: %d\n", status);
    previousStatus = status;
  }
}
