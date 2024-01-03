/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_
#define SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_

#include <supla/network/web_sender.h>
#include <supla/network/web_server.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#define ESPWebServer ESP8266WebServer
#elif defined(ESP32)
#include <WebServer.h>
#define ESPWebServer WebServer
#else
#error "Missing implementation for this target"
#endif

#include <supla/element.h>

namespace Supla {

class EspSender : public Supla::WebSender {
 public:
  explicit EspSender(::ESPWebServer *req);
  ~EspSender();
  void send(const char *, int) override;

 protected:
  ::ESPWebServer *reqHandler;
  bool error = false;
};

class EspWebServer : public Supla::WebServer, public Supla::Element {
 public:
  explicit EspWebServer(HtmlGenerator *generator = nullptr);
  virtual ~EspWebServer();
  void start() override;
  void stop() override;
  void iterateAlways() override;

  bool handlePost(bool beta = false);
  ::ESPWebServer *getServerPtr();

  bool dataSaved = false;

 protected:
  ::ESPWebServer server;
  bool started = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_
