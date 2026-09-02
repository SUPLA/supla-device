// SPDX-FileCopyrightText: Petione for AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_
#define SRC_SUPLA_NETWORK_HTML_BUTTON_UPDATE_H_

#if !defined(ARDUINO) && !defined(SUPLA_TEST)
#error "ButtonUpdate is Arduino-only and intended for debug/DIY usage."
#endif

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
