/*
 Copyright (C) Petione for AC SOFTWARE SP. Z O.O.

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

#ifndef ARDUINO_ARCH_AVR  // Exclude AVR (Arduino Mega)

#include "button_update.h"

#include <supla/network/web_sender.h>
#include <supla/network/esp_web_server.h>

using Supla::Html::ButtonUpdate;

ButtonUpdate::ButtonUpdate(Supla::EspWebServer* server)
    : HtmlElement(HTML_SECTION_BUTTON_AFTER), server(server) {
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
  sender->send(
      ("<button type=\"button\" onclick=\"window.location.href='/update';\">"
       "UPDATE"
       "</button>"));
}

#endif  // ARDUINO_ARCH_AVR

