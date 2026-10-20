// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file SupLanRemoteThermometer.ino
 * @brief Read one remote thermometer through SupLAN on ESP8266 or ESP32.
 * Wi-Fi and SUPLA settings are entered through the configuration web interface.
 * Serial is used only for logs. See README.md for provisioning instructions.
 * @tags suplan, remote_channel, thermometer, web_interface, esp8266, esp32
 */

#include <SuplaDevice.h>
#include <math.h>
#include <stdio.h>
#include <supla/protocol/suplan_protocol.h>
#include <supla/channels/remote_channel.h>
#include <supla/control/button.h>
#include <supla/device/status_led.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>

// Replace with the thermometer's global ChannelId from SUPLA Cloud.
// This is not its device-local ChannelNumber.
constexpr uint32_t REMOTE_THERMOMETER_CHANNEL_ID = 12345;
constexpr int CONFIG_BUTTON_GPIO = 0;
constexpr int STATUS_LED_GPIO = 2;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;
Supla::EspWebServer webServer;
Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);
Supla::RemoteChannel thermometer(REMOTE_THERMOMETER_CHANNEL_ID, Supla::READ);
bool deviceStarted = false;

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;

  auto configButton =
      new Supla::Control::Button(CONFIG_BUTTON_GPIO, true, true);
  configButton->configureAsConfigButton(&SuplaDevice);

  SuplaDevice.setName("SupLAN Remote Thermometer");
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  deviceStarted = SuplaDevice.enableSupLan() && SuplaDevice.begin(29);
  if (!deviceStarted) {
    Serial.println("SupLAN Device initialization failed");
  }
}

void loop() {
  if (!deviceStarted) return;
  SuplaDevice.iterate();

  static uint32_t lastLogMs = 0;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastLogMs) < 5000) return;
  lastLogMs = now;

  const auto state = thermometer.state();
  const double temperature = state.getValueDouble();
  bool peerReachable = false;
  bool transportReady = false;
  for (auto p = Supla::Protocol::ProtocolLayer::first(); p; p = p->next()) {
    if (auto resources = p->remoteResources()) {
      transportReady = p->isRegisteredAndReady();
      peerReachable = resources->peerAvailable(
          {Supla::SupLan::kResourceTypeChannel, REMOTE_THERMOMETER_CHANNEL_ID});
    }
  }
  const auto status = thermometer.accessStatus();
  char age[11] = "none";
  if (state.type()) snprintf(age, sizeof(age), "%lu",
      static_cast<unsigned long>(now - state.receivedMs()));
  Serial.printf("SupLAN ChannelId=%lu mode=SUPLAN_PEER access=%s "
                "D2D_transport=%s peer_session=%s "
                "snapshot_age_ms=%s ttl_s=%lu\n",
      static_cast<unsigned long>(REMOTE_THERMOMETER_CHANNEL_ID),
      Supla::channelAccessStatusName(status),
      transportReady ? "ready" : "unavailable",
      peerReachable ? "known" : "unavailable",
      age,
      static_cast<unsigned long>(state.validityTimeSec()));
  if (state.availableFor(Supla::ChannelCapability::Temperature) &&
      isfinite(temperature)) {
    Serial.printf("SupLAN remote temperature (cached snapshot): %.2f C\n", temperature);
  } else {
    Serial.println("SupLAN remote temperature unavailable");
  }
}
