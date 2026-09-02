// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_
#define SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_

#include <supla/network/html_output_buffer.h>
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
  static bool flushChunk(void *context, const char *buf, int size);
  ::ESPWebServer *reqHandler;
  HtmlOutputBuffer outputBuffer;
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
  char *getSendBufPtr() const;

  bool dataSaved = false;

 protected:
  ::ESPWebServer server;
  bool started = false;
  char *sendBuf = nullptr;
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_ESP_WEB_SERVER_H_
