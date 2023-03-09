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

#ifndef ARDUINO_ARCH_AVR

#include <SuplaDevice.h>
#include <stddef.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/network/html_element.h>
#include <supla/network/html_generator.h>
#include <supla/time.h>
#include <supla/tools.h>

#include "esp_web_server.h"

static Supla::EspWebServer *serverInstance = nullptr;
static int reboot = 0;

void getFavicon() {
  SUPLA_LOG_DEBUG("SERVER: get favicon.ico");
  if (serverInstance) {
    serverInstance->notifyClientConnected();
    auto svr = serverInstance->getServerPtr();
    svr->setContentLength(CONTENT_LENGTH_UNKNOWN);
    svr->send(200, "image/x-icon", "");
    serverInstance->getServerPtr()->sendContent((const char *)(Supla::favico),
                                                sizeof(Supla::favico));
  }
}

void getHandler() {
  SUPLA_LOG_DEBUG("SERVER: get request");

  if (serverInstance && serverInstance->htmlGenerator) {
    Supla::EspSender sender(serverInstance->getServerPtr());
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendPage(&sender, serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }
}

void getBetaHandler() {
  SUPLA_LOG_DEBUG("SERVER: get beta request");

  if (serverInstance && serverInstance->htmlGenerator) {
    Supla::EspSender sender(serverInstance->getServerPtr());
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendBetaPage(&sender,
                                                serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }
}

void postHandler() {
  SUPLA_LOG_DEBUG("SERVER: post request");
  if (serverInstance) {
    if (serverInstance->handlePost()) {
      // rtb/reboot == 1 is send by mobile applications. After such message
      // they disconnect from device's Wi-Fi, however ESP32 WebServer
      // implementation will try to send each chunk of HTML from getHanlder
      // with very long timeout. In order to prevent app from hanging, we
      // skip getHandler in such case.
      if (reboot != 1) {
        getHandler();
      }
    }
  }
}

void postBetaHandler() {
  SUPLA_LOG_DEBUG("SERVER: post request");
  if (serverInstance) {
    if (serverInstance->handlePost()) {
      getBetaHandler();
    }
  }
}

::ESPWebServer *Supla::EspWebServer::getServerPtr() {
  return &server;
}

Supla::EspWebServer::EspWebServer(Supla::HtmlGenerator *generator)
    : WebServer(generator), server(80) {
  serverInstance = this;
}

Supla::EspWebServer::~EspWebServer() {
  serverInstance = nullptr;
}

bool Supla::EspWebServer::handlePost() {
  notifyClientConnected();
  resetParser();

  for (int i = 0; i < server.args(); i++) {
    SUPLA_LOG_DEBUG(
              "SERVER: key %s, value %s",
              server.argName(i).c_str(),
              server.arg(i).c_str());
    for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
         htmlElement = htmlElement->next()) {
      if (htmlElement->handleResponse(server.argName(i).c_str(),
                                      server.arg(i).c_str())) {
        break;
      }
    }
    if (strcmp(server.argName(i).c_str(), "rbt") == 0) {
      reboot = stringToUInt(server.arg(i).c_str());
      SUPLA_LOG_DEBUG("rbt found %d", reboot);
      if (reboot == 2) {
        sdc->scheduleSoftRestart(2500);
      } else if (reboot) {
        sdc->scheduleSoftRestart(1000);
      }
    }
    delay(1);
  }

  for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
      htmlElement = htmlElement->next()) {
    htmlElement->onProcessingEnd();
  }

  if (Supla::Storage::ConfigInstance()) {
    Supla::Storage::ConfigInstance()->commit();
    sdc->disableLocalActionsIfNeeded();
  }

  // call getHandler to generate config html page
  serverInstance->dataSaved = true;
  return true;
}

void Supla::EspWebServer::start() {
  if (started) {
    return;
  }

  started = true;

  SUPLA_LOG_INFO("Starting local web server");

  server.on("/", HTTP_GET, getHandler);
  server.on("/beta", HTTP_GET, getBetaHandler);
  server.on("/favicon.ico", HTTP_GET, getFavicon);
  server.on("/", HTTP_POST, postHandler);
  server.on("/beta", HTTP_POST, postBetaHandler);

  server.begin();
}

void Supla::EspWebServer::stop() {
  SUPLA_LOG_INFO("Stopping local web server");
  if (started) {
    server.stop();
    started = false;
  }
}

Supla::EspSender::EspSender(::ESPWebServer *req) : reqHandler(req) {
  reqHandler->setContentLength(CONTENT_LENGTH_UNKNOWN);
  reqHandler->send(200, "text/html", "");
}

Supla::EspSender::~EspSender() {
}

void Supla::EspSender::send(const char *buf, int size) {
  if (error || !buf || !reqHandler) {
    return;
  }
  if (!reqHandler->client().connected()) {
    SUPLA_LOG_WARNING("WebSender error - lost connection");
    error = true;
    return;
  }

  if (size == -1) {
    size = strlen(buf);
  }
  if (size == 0) {
    return;
  }

  reqHandler->sendContent(buf, size);
}

void Supla::EspWebServer::iterateAlways() {
  if (started) {
    server.handleClient();
  }
}
#endif
