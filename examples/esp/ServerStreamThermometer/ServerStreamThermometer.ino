// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file ServerStreamThermometer.ino
 * @brief Read a thermometer over SUPLA Server Stream on ESP8266 or ESP32.
 * Wi-Fi and SUPLA settings are entered through the configuration web interface.
 * Serial is output-only. See README.md for TLS and READ approval instructions.
 * @tags server_stream, remote_channel, thermometer, web_interface, esp8266, esp32
 */

#include <SuplaDevice.h>
#include <inttypes.h>
#include <math.h>
#include <supla/channels/server_remote_channel.h>
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
constexpr uint32_t THERMOMETER_CHANNEL_ID = 12345;
constexpr int CONFIG_BUTTON_GPIO = 0;
constexpr int STATUS_LED_GPIO = 2;

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;
Supla::EspWebServer webServer;
Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);
Supla::ServerRemoteChannel thermometer(THERMOMETER_CHANNEL_ID);
bool deviceStarted = false;

void setup() {
  Serial.begin(115200);

  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;

  auto configButton =
      new Supla::Control::Button(CONFIG_BUTTON_GPIO, true, true);
  configButton->configureAsConfigButton(&SuplaDevice);

  SuplaDevice.setName("Server Stream Thermometer");
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  // The SupLAN capability flag and SERVER identity/sync are also required for
  // Server Stream. SDK initialization and registration retain the TLS guard.
  deviceStarted = SuplaDevice.enableSupLan() && SuplaDevice.begin(29);
  if (!deviceStarted) {
    Serial.println("SUPLA initialization failed");
  }
}

void loop() {
  if (!deviceStarted) return;
  SuplaDevice.iterate();

  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastPrintMs) < 5000) return;
  lastPrintMs = now;

  const auto status = thermometer.accessStatus();
  // Borrowed snapshot: use it immediately, without keeping it across iterate().
  const auto state = thermometer.state();
  const double temperature = state.getValueDouble();

  Serial.printf("Server Stream ChannelId=%" PRIu32 ", access=%s: ",
                THERMOMETER_CHANNEL_ID,
                Supla::channelAccessStatusName(status));
  if (state.availableFor(Supla::ChannelCapability::Temperature) &&
      isfinite(temperature)) {
    Serial.printf("%.2f C\n", temperature);
  } else {
    Serial.println("temperature unavailable");
  }
}
