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

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_

#include <supla/network/html_element.h>

namespace Supla {
class EspWebServer;
}  // namespace Supla

#ifdef ARDUINO_ARCH_ESP32
#include <HTTPUpdateServer.h>
#else
#include <ESP8266HTTPUpdateServer.h>
#endif

namespace Supla {
namespace Html {

class ButtonUpdate : public HtmlElement {
 private:
  Supla::EspWebServer* server = nullptr;

#ifdef ARDUINO_ARCH_ESP32
  HTTPUpdateServer* httpUpdater = nullptr;
#else
  ESP8266HTTPUpdateServer* httpUpdater = nullptr;
#endif

 public:
  explicit ButtonUpdate(Supla::EspWebServer* server);
  ~ButtonUpdate();
  void send(Supla::WebSender* sender) override;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_
