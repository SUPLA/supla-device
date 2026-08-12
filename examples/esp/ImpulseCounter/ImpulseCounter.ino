// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @supla-example
 * @file ImpulseCounter.ino
 * @brief Example of implementing impulse counters with optional LED feedback for SUPLA using an ESP8266/ESP32.
 * This example demonstrates how to configure two impulse counters on different pins (counting rising or falling edges, with debounce).
 * It also shows how to trigger an internal LED (blink or toggle) upon each impulse count.
 * The device integrates with the SUPLA cloud via Wi-Fi and includes a web server for configuration.
 * Network settings are configured via the web interface.
 * It can store counter data persistently in EEPROM (or optionally FRAM).
 * Users need to adjust GPIO pins for the impulse counters and LEDs in the code.
 * A status LED is also configured.
 *
 * @tags impulse, counter, LED, debounce, EEPROM, FRAM, esp, esp32, esp8266, wifi, web_interface
 */
#include <SuplaDevice.h>
#include <supla/sensor/impulse_counter.h>
#include <supla/actions.h>
#include <supla/control/internal_pin_output.h>
#include <supla/device/status_led.h>
#include <supla/events.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/esp_wifi.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/storage/littlefs_config.h>

// Choose where Supla should store state data in persistent memory
// We recommend to use external FRAM memory
// #define FRAM_CS_PIN TBD // choose a free GPIO and replace TBD
// #define STORAGE_OFFSET 100
// #include <supla/storage/fram_spi.h>
// Supla::FramSpi fram(FRAM_CS_PIN, STORAGE_OFFSET);
#include <supla/storage/eeprom.h>
Supla::Eeprom eeprom;

#define STATUS_LED_GPIO 2

Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true);  // inverted state
Supla::EspWebServer suplaServer;

void setup() {
  Serial.begin(115200);

  // HTML www component
  new Supla::Html::DeviceInfo(&SuplaDevice);
  new Supla::Html::WifiParameters;
  new Supla::Html::ProtocolParameters;
  new Supla::Html::StatusLedParameters;

  // CHANNEL0 - Impulse Counter on pin 34, counting raising edge (from LOW to
  // HIGH), no pullup on pin, and 10 ms debounce timeout
  auto ic1 = new Supla::Sensor::ImpulseCounter(34, true, false, 10);

  // CHANNEL1 - Impulse Counter on pin 35, counting falling edge (from HIGH to
  // LOW), with pullup on pin, and 50 ms debounce timeout
  auto ic2 = new Supla::Sensor::ImpulseCounter(35, false, true, 50);

  // Configuring internal LED to notify each change of impulse counter
  auto led1 = new Supla::Control::InternalPinOutput(24);  // LED on pin 24
  auto led2 = new Supla::Control::InternalPinOutput(25);  // LED on pin 25

  // LED1 will blink (100 ms) on each change of ic1
  led1->setDurationMs(100);
  ic1->addAction(Supla::TURN_ON, led1, Supla::ON_CHANGE);

  // LED2 will toggle it's state on each change of ic2
  ic2->addAction(Supla::TOGGLE, led2, Supla::ON_CHANGE);

  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
  SuplaDevice.begin();
}

void loop() {
  SuplaDevice.iterate();
}
