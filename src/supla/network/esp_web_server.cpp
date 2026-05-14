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
#include <supla/storage/storage.h>
#include <supla/storage/config.h>

#include "esp_web_server.h"

static Supla::EspWebServer *serverInstance = nullptr;

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
      // Keep the HTTP transaction complete even when a restart is scheduled.
      // The restart is delayed by handlePost(), so the redirect can still be
      // delivered before the device goes away.
      serverInstance->getServerPtr()->sendHeader("Location", "/", true);
      serverInstance->getServerPtr()->send(
          303, "text/plain", "Redirecting...");
    } else {
      serverInstance->getServerPtr()->send(
          400, "text/plain", "Invalid POST request");
    }
  }
}

void postBetaHandler() {
  SUPLA_LOG_DEBUG("SERVER: beta post request");
  if (serverInstance) {
    if (serverInstance->handlePost(true)) {
      serverInstance->getServerPtr()->sendHeader("Location", "/beta", true);
      serverInstance->getServerPtr()->send(
          303, "text/plain", "Redirecting...");
    } else {
      serverInstance->getServerPtr()->send(
          400, "text/plain", "Invalid POST request");
    }
  }
}

::ESPWebServer *Supla::EspWebServer::getServerPtr() {
  return &server;
}

char *Supla::EspWebServer::getSendBufPtr() const {
  return sendBuf;
}

Supla::EspWebServer::EspWebServer(Supla::HtmlGenerator *generator)
    : WebServer(generator), server(80) {
  serverInstance = this;
}

Supla::EspWebServer::~EspWebServer() {
  serverInstance = nullptr;
  if (sendBuf) {
    delete[] sendBuf;
    sendBuf = nullptr;
  }
}

bool Supla::EspWebServer::handlePost(bool beta) {
  notifyClientConnected(true);
  resetParser();
  if (beta) {
    setBetaProcessing();
  }

  if (server.args() <= 0 || strcmp(server.argName(0).c_str(), "csrf") != 0 ||
      !isCsrfTokenValid(server.arg(0).c_str())) {
    SUPLA_LOG_WARNING("SERVER: invalid CSRF token");
    return false;
  }

  for (int i = 1; i < server.args(); i++) {
    if (strcmp(server.argName(i).c_str(), "csrf") == 0) {
      continue;
    }
    char redactedValue[Supla::REDACTED_LOG_VALUE_BUFFER_SIZE] = {};
    SUPLA_LOG_DEBUG(
              "SERVER: key %s, value %s",
              server.argName(i).c_str(),
              Supla::redactLogValue(server.argName(i).c_str(),
                                    server.arg(i).c_str(),
                                    redactedValue,
                                    sizeof(redactedValue)));
    for (auto htmlElement = Supla::HtmlElement::begin(); htmlElement;
         htmlElement = htmlElement->next()) {
      if (isSectionAllowed(htmlElement->section)) {
        if (htmlElement->handleResponse(server.argName(i).c_str(),
                                        server.arg(i).c_str())) {
          break;
        }
      }
    }
    if (strcmp(server.argName(i).c_str(), "rbt") == 0) {
      int reboot = stringToUInt(server.arg(i).c_str());
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
    if (isSectionAllowed(htmlElement->section)) {
      htmlElement->onProcessingEnd();
    }
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
  if (!sendBuf) {
    sendBuf = new char[SUPLA_HTML_OUTPUT_BUFFER_SIZE];
    if (sendBuf) {
      memset(sendBuf, 0, SUPLA_HTML_OUTPUT_BUFFER_SIZE);
    }
  }

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

Supla::EspSender::EspSender(::ESPWebServer *req)
    : reqHandler(req),
      outputBuffer(serverInstance ? serverInstance->getSendBufPtr() : nullptr,
                   SUPLA_HTML_OUTPUT_BUFFER_SIZE) {
  reqHandler->setContentLength(CONTENT_LENGTH_UNKNOWN);
  reqHandler->send(200, "text/html", "");
}

Supla::EspSender::~EspSender() {
  if (reqHandler && !outputBuffer.error()) {
    outputBuffer.flush(reqHandler, &EspSender::flushChunk);
  }
}

bool Supla::EspSender::flushChunk(void *context, const char *buf, int size) {
  if (context == nullptr || buf == nullptr || size < 0) {
    return false;
  }
  auto *req = reinterpret_cast<::ESPWebServer *>(context);
  if (!req->client().connected()) {
    SUPLA_LOG_WARNING("WebSender error - lost connection");
    return false;
  }
  req->sendContent(buf, size);
  return true;
}

void Supla::EspSender::send(const char *buf, int size) {
  if (!buf || !reqHandler) {
    return;
  }
  outputBuffer.send(reqHandler, &EspSender::flushChunk, buf, size);
}

void Supla::EspWebServer::iterateAlways() {
  if (started) {
    server.handleClient();
  }
}
#endif
