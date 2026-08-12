// SPDX-FileCopyrightText: Petione for AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#if !defined(ARDUINO) && !defined(SUPLA_TEST)
#error "ButtonUpdate is Arduino-only and intended for debug/DIY usage."
#endif

#ifndef ARDUINO_ARCH_AVR  // Exclude AVR (Arduino Mega)

#include "button_update.h"

#include <supla/network/web_sender.h>
#include <supla/network/esp_web_server.h>
#include <supla/log_wrapper.h>

using Supla::Html::ButtonUpdate;

ButtonUpdate::ButtonUpdate(Supla::EspWebServer* server)
    : HtmlElement(HTML_SECTION_BUTTON_AFTER), server(server) {
  SUPLA_LOG_WARNING(
      "ButtonUpdate: registering unauthenticated OTA endpoint at /update "
      "(debug/DIY only)");
#ifdef ARDUINO_ARCH_ESP32
  httpUpdater = new HTTPUpdateServer();
#else
  httpUpdater = new ESP8266HTTPUpdateServer();
#endif

  // Automatic OTA update setup
  httpUpdater->setup(server->getServerPtr(), "/update");
}

ButtonUpdate::~ButtonUpdate() {
  delete httpUpdater;
  httpUpdater = nullptr;
}

void ButtonUpdate::send(Supla::WebSender* sender) {
  SUPLA_LOG_WARNING(
      "ButtonUpdate: rendering unauthenticated OTA button "
      "(debug/DIY only)");
  sender->send(
      ("<button type=\"button\" onclick=\"window.location.href='/update';\">"
       "UPDATE"
       "</button>"));
}

#endif  // ARDUINO_ARCH_AVR
